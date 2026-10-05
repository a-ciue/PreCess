/** @file TestSessionJobs.cpp
 * @brief 无 Qt 宿主的任务拥有关系、终态重放与拆解回归。
 */
#include "AlgorithmHandler.h"
#include "AlgorithmSystem.h"
#include "ArgObject.h"
#include "FeatureHandler.h"
#include "FeatureRegistrar.h"
#include "FeatureSystem.h"
#include "JobRunner.h"
#include "MeshData.h"
#include "ModelScope.h"
#include "Session.h"
#include "UndoStack.h"
#include "test/OwnerQueue.h"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
using namespace systems::job;
namespace {
Index seed(session::Session& session)
{
    Index target;
    {
        ModelScope scope(session.model(), &session.undoStack(), "seed");
        auto component = std::make_unique<ComponentData>();
        component->mesh = std::make_unique<MeshData>();
        component->mesh->vertex_positions_.push_back({ 0, 0, 0 });
        ComponentDatas components;
        components.push_back(std::move(component));
        auto id = session.model().addModel("test", std::move(components));
        target = session.model().modelById(id)->componentIds().front();
    }
    session.undoStack().clear();
    return target;
}
class Feature final : public systems::feature::FeatureHandler {
public:
    Index target { -1 };
    int notifications { 0 };
    JobTaskFn compute;
    std::function<void()> cleanup;
    std::function<void(systems::feature::FeatureContext&)> on_notification;
    std::shared_ptr<Job> job;
    core::EventBus::Subscription subscription;
    void setup(systems::feature::FeatureRegistrar&, systems::feature::FeatureContext& ctx) override
    {
        subscription = ctx.events.subscribe<systems::feature::ModelEvent>([this, &ctx](const auto& event) {
            if (event.kind == systems::feature::ModelEvent::Kind::ComponentChanged) {
                ++notifications;
                if (on_notification)
                    on_notification(ctx);
            }
        });
    }
    std::any execute(systems::feature::FeatureContext& ctx) override
    {
        if (compute)
            job = ctx.runJob("pure", compute);
        else
            job = ctx.runModelJob("feature", target, [](ComponentOperator& op, ProgressFn) {
                op.appendPoint({ 1, 0, 0 });
            });
        return { };
    }
    void teardown(systems::feature::FeatureContext&) override
    {
        if (cleanup)
            cleanup();
    }
};
Feature& installFeature(session::Session& session, Index target)
{
    auto feature = std::make_unique<Feature>();
    auto* raw = feature.get();
    raw->target = target;
    systems::feature::HandlerMetaData meta;
    meta.name = "Feature";
    REQUIRE(session.featureSystem().registerHandler(meta,
        systems::feature::FeatureSystem::SystemHandlerPtr { feature.release() }));
    return *raw;
}
class Algorithm final : public systems::algo::AlgorithmHandler {
public:
    std::vector<core::ArgType> args_type() const override { return { }; }
    std::any execute(systems::algo::HandlerContext& ctx, const std::vector<core::ArgObject>&) override
    {
        ctx.cur_component.appendPoint({ 2, 0, 0 });
        return { };
    }
};
}
TEST_CASE("Native session replays feature and algorithm notifications despite observer failure", "[session][job]")
{
    OwnerQueue queue;
    session::Session session(nullptr, queue.dispatcher());
    const auto target = seed(session);
    auto& feature = installFeature(session, target);
    int finished = 0;
    session.setTaskCallbacks({ }, { }, [&](Job&) {
        CHECK_FALSE(session.model().writesPending());
        CHECK(feature.notifications == 0);
        ++finished;
        throw std::runtime_error("display observer failed");
    });
    std::shared_ptr<Job> job;
    SECTION("feature")
    {
        session.featureSystem().invoke("Feature");
        job = feature.job;
    }
    SECTION("algorithm")
    {
        systems::algo::HandlerMetaData meta { "Algorithm", "algorithm" };
        REQUIRE(session.algorithmSystem().registerHandler(meta,
            systems::algo::AlgorithmSystem::SystemHandlerPtr { std::make_unique<Algorithm>().release() }));
        job = session.algorithmSystem().callAsync("Algorithm", target, { });
        REQUIRE_THROWS_AS(session.algorithmSystem().unregisterHandler(meta), ModelOperationBusy);
        REQUIRE_THROWS_AS(session.algorithmSystem().registerHandler(meta,
                              systems::algo::AlgorithmSystem::SystemHandlerPtr { std::make_unique<Algorithm>().release() }),
            ModelOperationBusy);
        REQUIRE(session.algorithmSystem().getArgTypes("Algorithm"));
    }
    REQUIRE(job);
    auto completion = queue.take();
    CHECK(feature.notifications == 0);
    CHECK_FALSE(session.undoStack().undo());
    completion();
    REQUIRE(job->state() == JobState::Done);
    REQUIRE(finished == 1);
    REQUIRE(feature.notifications == 1);
    REQUIRE(session.model().getComponentOperator(target)->mesh()->vertex_positions_.size() == 2);
    completion();
    REQUIRE(finished == 1);
    REQUIRE(session.undoStack().undo());
    REQUIRE(session.model().getComponentOperator(target)->mesh()->vertex_positions_.size() == 1);
    REQUIRE_FALSE(session.undoStack().canUndo());
}
TEST_CASE("Session notification replay may publish a successor without releasing it on old completion", "[session][job]")
{
    OwnerQueue queue;
    session::Session session(nullptr, queue.dispatcher());
    const auto target = seed(session);
    auto& feature = installFeature(session, target);
    std::shared_ptr<Job> successor;
    feature.on_notification = [&](systems::feature::FeatureContext& ctx) {
        successor = ctx.runJob("successor", [](ProgressFn) { });
    };
    session.featureSystem().invoke("Feature");
    auto first = feature.job;
    auto completion = queue.take();
    completion();
    REQUIRE(first->state() == JobState::Done);
    REQUIRE(successor);
    REQUIRE(session.jobRunner()->currentJob() == successor);
    REQUIRE(session.model().writesPending());
    completion();
    REQUIRE(session.jobRunner()->currentJob() == successor);
    queue.take()();
    REQUIRE(successor->state() == JobState::Done);
    REQUIRE_FALSE(session.model().writesPending());
    REQUIRE(feature.notifications == 1);
}
TEST_CASE("Session destruction joins calculation before feature teardown and invalidates late completion", "[session][job]")
{
    OwnerQueue queue;
    auto session = std::make_unique<session::Session>(nullptr, queue.dispatcher());
    auto& feature = installFeature(*session, seed(*session));
    std::mutex mutex;
    std::condition_variable changed;
    bool entered = false, released = false;
    std::atomic_bool returned { false };
    bool cleaned = false;
    const auto owner = std::this_thread::get_id();
    feature.compute = [&](ProgressFn) {
        std::unique_lock lock(mutex);
        entered = true;
        changed.notify_all();
        changed.wait(lock, [&] { return released; });
        returned = true;
    };
    feature.cleanup = [&] {
        CHECK(std::this_thread::get_id() == owner);
        CHECK(returned.load());
        cleaned = true;
    };
    session->featureSystem().invoke("Feature");
    auto job = feature.job;
    {
        std::unique_lock lock(mutex);
        REQUIRE(changed.wait_for(lock, std::chrono::seconds(5), [&] { return entered; }));
    }
    std::thread releaser([&] {
        while (!job->isCancellationRequested())
            std::this_thread::yield();
        {
            std::lock_guard lock(mutex);
            released = true;
        }
        changed.notify_all();
    });
    session.reset();
    releaser.join();
    REQUIRE(cleaned);
    REQUIRE(job->state() == JobState::Cancelled);
    REQUIRE_NOTHROW(queue.take()());
}
TEST_CASE("Native synchronous session has no asynchronous runner", "[session][job]")
{
    session::Session session;
    REQUIRE_FALSE(session.jobRunner());
    REQUIRE_FALSE(session.algorithmSystem().callAsync("unavailable", -1, { }));
}
