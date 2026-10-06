#include "JobRunner.h"
#include "ModelLayer.h"
#include "ModelScope.h"
#include "UndoStack.h"
#include "test/OwnerQueue.h"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>
using namespace systems::job;

TEST_CASE("Inline task computes and completes on owner without dispatch", "[JobRunner]")
{
    int dispatched = 0;
    ModelLayer model;
    JobRunner runner(model, nullptr, [&](std::function<void()>) { ++dispatched; });
    int progress = 0, finished = 0, value = 0;
    runner.setOnProgress([&](Job&, double value, const std::string&) { CHECK(value == 0.5); ++progress; });
    runner.setOnFinished([&](Job&) { ++finished; });
    auto job = runner.run("inline", [&] { return systems::job::JobWork { [&](ProgressFn report) {
        report(0.5, "half"); value = 42; }, [&](ProgressFn) { CHECK(value == 42); }, true }; }, { });
    REQUIRE(job->state() == JobState::Done);
    REQUIRE(progress == 1);
    REQUIRE(finished == 1);
    REQUIRE(dispatched == 0);
}
TEST_CASE("Computation and commit failures preserve their errors", "[JobRunner]")
{
    ModelLayer model;
    OwnerQueue queue;
    JobRunner runner(model, nullptr, queue.dispatcher());
    bool applied = false;
    SECTION("throw")
    {
        auto job = runner.run("throw", [&] { return systems::job::JobWork { [](ProgressFn) -> void { throw std::runtime_error("compute failed"); }, [&](ProgressFn) { applied = true; }, true }; }, { });
        REQUIRE(job->state() == JobState::Failed);
        REQUIRE(job->error() == "compute failed");
    }

    SECTION("commit throw")
    {
        auto job = runner.run("commit", [&] { return systems::job::JobWork { [](ProgressFn) { }, [](ProgressFn) { throw std::runtime_error("commit failed"); }, true }; }, { });
        REQUIRE(job->state() == JobState::Failed);
        REQUIRE(job->error() == "commit failed");
    }

    REQUIRE_FALSE(applied);
}
TEST_CASE("Heartbeat cancellation stops task at next report", "[JobRunner]")
{
    ModelLayer model;
    OwnerQueue queue;
    JobRunner runner(model, nullptr, queue.dispatcher());
    bool returned = false;
    int count = 0;
    runner.setOnProgress([&](Job& j, double, const std::string&) { ++count; j.cancel(); });
    auto job = runner.run("cancel", [&] { return systems::job::JobWork { [&](ProgressFn report) {
        report(0.1, "first"); report(0.2, "second"); returned = true; }, { }, true }; }, { });
    REQUIRE(job->state() == JobState::Cancelled);
    REQUIRE(count == 1);
    REQUIRE_FALSE(returned);
}
TEST_CASE("Computed result retains slot and model authority until one owner completion", "[JobRunner][operation]")
{
    OwnerQueue queue;
    ModelLayer model;
    JobRunner runner(model, nullptr, queue.dispatcher());
    const auto owner = std::this_thread::get_id();
    std::thread::id compute_thread, commit_thread, finish_thread;
    runner.setOnFinished([&](Job&) { finish_thread = std::this_thread::get_id(); CHECK_FALSE(model.writesPending()); });
    auto job = runner.run("first", [&] { return systems::job::JobWork { [&](ProgressFn) { compute_thread = std::this_thread::get_id(); }, [&](ProgressFn) { commit_thread = std::this_thread::get_id(); CHECK(runner.currentJob()); } }; }, { });
    auto notification = queue.take();
    REQUIRE(compute_thread != owner);
    REQUIRE(job->state() == JobState::Running);
    REQUIRE(runner.currentJob() == job);
    REQUIRE(model.writesPending());
    REQUIRE_FALSE(runner.run("second", [&] { return systems::job::JobWork { [](ProgressFn) { }, { } }; }, { }));
    REQUIRE_FALSE(runner.run("third", [&] { return systems::job::JobWork { [](ProgressFn) { }, { }, true }; }, { }));
    notification();
    REQUIRE(commit_thread == owner);
    REQUIRE(finish_thread == owner);
    REQUIRE(job->state() == JobState::Done);
    REQUIRE_FALSE(runner.currentJob());
    notification();
    REQUIRE_FALSE(model.writesPending());
}
TEST_CASE("Queued cancellation discards result and still drains cleanup", "[JobRunner][operation]")
{
    OwnerQueue queue;
    ModelLayer model;
    JobRunner runner(model, nullptr, queue.dispatcher());
    bool applied = false;
    int cleaned = 0;
    auto job = runner.run("queued", [&] { return systems::job::JobWork { [](ProgressFn) { }, [&](ProgressFn) { applied = true; } }; }, { });
    auto fn = queue.take();
    REQUIRE(runner.deferUntilFinished([&] {
        CHECK(model.writesPending());
        ++cleaned;
        CHECK(runner.deferUntilFinished([&] { CHECK(model.writesPending()); ++cleaned; }));
    }));
    job->cancel();
    REQUIRE(model.writesPending());
    fn();
    REQUIRE(job->state() == JobState::Cancelled);
    REQUIRE_FALSE(applied);
    REQUIRE(cleaned == 2);
    REQUIRE_FALSE(model.writesPending());
}
TEST_CASE("No-heartbeat cancellation keeps authority until actual return", "[JobRunner][operation]")
{
    OwnerQueue queue;
    ModelLayer model;
    JobRunner runner(model, nullptr, queue.dispatcher());
    std::mutex mutex;
    std::condition_variable changed;
    bool entered = false, release = false;
    auto job = runner.run("blocking", [&] { return systems::job::JobWork { [&](ProgressFn) {
                                                                              std::unique_lock lock(mutex);
                                                                              entered = true;
                                                                              changed.notify_one();
                                                                              changed.wait(lock, [&] { return release; });
                                                                          },
                                                { } }; },
        { });
    {
        std::unique_lock lock(mutex);
        REQUIRE(changed.wait_for(lock, std::chrono::seconds(5), [&] { return entered; }));
    }
    job->cancel();
    REQUIRE(model.writesPending());
    REQUIRE_FALSE(isTerminal(job->state()));
    {
        std::lock_guard lock(mutex);
        release = true;
    }
    changed.notify_one();
    queue.take()();
    REQUIRE(job->state() == JobState::Cancelled);
    REQUIRE_FALSE(model.writesPending());
}
TEST_CASE("Stop joins without GUI processing and late callback is harmless", "[JobRunner][operation]")
{
    OwnerQueue queue;
    ModelLayer model;
    bool applied = false, cleaned = false;
    std::shared_ptr<Job> job;
    std::function<void()> late;
    {
        JobRunner runner(model, nullptr, queue.dispatcher());
        job = runner.run("stop", [&] { return systems::job::JobWork { [](ProgressFn) { }, [&](ProgressFn) { applied = true; } }; }, { });
        late = queue.take();
        REQUIRE(runner.deferUntilFinished([&] { CHECK(model.writesPending()); cleaned = true; }));
        runner.stop();
        REQUIRE(job->state() == JobState::Cancelled);
        REQUIRE(cleaned);
        REQUIRE_FALSE(model.writesPending());
    }
    late();
    REQUIRE_FALSE(applied);
}
TEST_CASE("Old completion cannot release successor submitted by finished callback", "[JobRunner][operation]")
{
    OwnerQueue queue;
    ModelLayer model;
    JobRunner runner(model, nullptr, queue.dispatcher());
    std::shared_ptr<Job> successor;
    runner.setOnFinished([&](Job& job) {
        if (job.name() == "first")
            successor = runner.run("second", [] { return JobWork { [](ProgressFn) { } }; });
    });
    auto first = runner.run("first", [&] { return systems::job::JobWork { [](ProgressFn) { }, { } }; }, { });
    auto old = queue.take();
    old();
    REQUIRE(successor);
    REQUIRE(first->state() == JobState::Done);
    old();
    REQUIRE(runner.currentJob() == successor);
    REQUIRE(model.writesPending());
    queue.take()();
    REQUIRE(successor->state() == JobState::Done);
}
TEST_CASE("Preparation runs occupied and exception or refusal releases reservation", "[JobRunner][operation]")
{
    ModelLayer model;
    OwnerQueue queue;
    JobRunner runner(model, nullptr, queue.dispatcher());
    REQUIRE_THROWS_AS(runner.run("bad", [&]() -> JobWork {
        CHECK(model.writesPending());
        throw std::runtime_error("prepare failed");
    }),
        std::runtime_error);
    REQUIRE_FALSE(model.writesPending());
    REQUIRE_FALSE(runner.currentJob());
    REQUIRE_FALSE(runner.run("refused", [] { return JobWork { }; }));
    REQUIRE_FALSE(model.writesPending());
    auto job = runner.run("next", [&] { return systems::job::JobWork { [](ProgressFn) { }, { }, true }; }, { });
    REQUIRE(job->state() == JobState::Done);
}
TEST_CASE("Runner construction requires an owner dispatcher", "[JobRunner]")
{
    ModelLayer model;
    REQUIRE_THROWS_AS((JobRunner { model, nullptr, { } }), std::runtime_error);
    REQUIRE_FALSE(model.writesPending());
}
TEST_CASE("Runner injection rejects a foreign model or undo host", "[JobRunner][operation]")
{
    ModelLayer model, foreign_model;
    UndoStack stack(model), foreign_stack(foreign_model);
    OwnerQueue queue;
    JobRunner runner(model, &stack, queue.dispatcher());
    REQUIRE_NOTHROW(runner.assertHost(model, &stack));
    REQUIRE_THROWS_AS(runner.assertHost(foreign_model, &stack), std::runtime_error);
    REQUIRE_THROWS_AS(runner.assertHost(model, &foreign_stack), std::runtime_error);
    REQUIRE_THROWS_AS(runner.assertHost(model, nullptr), std::runtime_error);
    auto job = runner.run("correct host", [] { return JobWork { [](ProgressFn) { } }; });
    REQUIRE(model.writesPending());
    REQUIRE_FALSE(foreign_model.writesPending());
    queue.take()();
    REQUIRE(job->state() == JobState::Done);
    REQUIRE_FALSE(model.writesPending());
}
TEST_CASE("Owner-thread contract rejects off-thread publication and stop", "[JobRunner]")
{
    ModelLayer model;
    OwnerQueue queue;
    JobRunner runner(model, nullptr, queue.dispatcher());
    bool submit_rejected = false, stop_rejected = false;
    std::thread other([&] {
        try {
            runner.run("foreign", [&] { return systems::job::JobWork { [](ProgressFn) { }, { }, true }; }, { });
        } catch (const std::runtime_error&) {
            submit_rejected = true;
        }
        try {
            runner.stop();
        } catch (const std::runtime_error&) {
            stop_rejected = true;
        }
    });
    other.join();
    REQUIRE(submit_rejected);
    REQUIRE(stop_rejected);
}
TEST_CASE("Completion rejects an open boundary once; inline child shares it", "[JobRunner][G3]")
{
    OwnerQueue queue;
    ModelLayer model;
    UndoStack stack(model);
    JobRunner runner(model, &stack, queue.dispatcher());
    bool applied = false;
    stack.beginOperation("outer");
    auto job = runner.run("async", [&] { return systems::job::JobWork { [](ProgressFn) { }, [&](ProgressFn) { applied = true; } }; }, { });
    auto fn = queue.take();
    fn();
    stack.commitOperation();
    REQUIRE_FALSE(applied);
    REQUIRE(job->state() == JobState::Failed);
    REQUIRE(job->error().find("boundary") != std::string::npos);
    {
        ModelScope outer(model, &stack, "inline outer");
        auto child = runner.run("child", [&] { return systems::job::JobWork { [](ProgressFn) { }, [&](ProgressFn) { applied = true; }, true }; }, { });
        REQUIRE(child->state() == JobState::Done);
    }
    REQUIRE(applied);
}

TEST_CASE("Stop joins computing worker and cleans on owner without an event pump", "[JobRunner][operation]")
{
    OwnerQueue queue;
    ModelLayer model;
    JobRunner runner(model, nullptr, queue.dispatcher());
    std::mutex mutex;
    std::condition_variable changed;
    bool entered = false;
    bool released = false;
    bool cleaned = false;
    const auto owner = std::this_thread::get_id();
    auto job = runner.run("computing", [&] { return systems::job::JobWork { [&](ProgressFn) {
        std::unique_lock lock(mutex);
        entered = true;
        changed.notify_all();
        changed.wait(lock, [&] { return released; }); }, [](ProgressFn) { FAIL("stop must not apply result"); } }; }, { });
    {
        std::unique_lock lock(mutex);
        REQUIRE(changed.wait_for(lock, std::chrono::seconds(5), [&] { return entered; }));
    }
    runner.deferUntilFinished([&] {
        CHECK(std::this_thread::get_id() == owner);
        CHECK(model.writesPending());
        cleaned = true;
    });
    std::thread releaser([&] {
        // 等待 stop 请求取消，才允许无心跳计算实际返回。
        while (!job->isCancellationRequested())
            std::this_thread::yield();
        {
            std::lock_guard lock(mutex);
            released = true;
        }
        changed.notify_all();
    });
    runner.stop();
    releaser.join();
    REQUIRE(cleaned);
    REQUIRE(job->state() == JobState::Cancelled);
    REQUIRE_FALSE(model.writesPending());
    queue.take()(); // 已收尾的旧通知空转。
}

TEST_CASE("Task and apply captures are destroyed on owner before handler cleanup", "[JobRunner][lifetime]")
{
    OwnerQueue queue;
    ModelLayer model;
    JobRunner runner(model, nullptr, queue.dispatcher());
    std::thread::id destroyed_on;
    struct Payload {
        std::thread::id* destroyed_on;
        ~Payload() { *destroyed_on = std::this_thread::get_id(); }
    };
    auto payload = std::make_shared<Payload>();
    payload->destroyed_on = &destroyed_on;
    std::weak_ptr<Payload> alive = payload;
    bool fail = false;
    SECTION("successful apply") { }
    SECTION("failed compute result") { fail = true; }
    auto job = runner.run("captures", [&] { return systems::job::JobWork { [payload, fail](ProgressFn) {
        if (fail) throw std::runtime_error("failed"); }, [payload](ProgressFn) { } }; }, { });
    payload.reset();
    auto completion = queue.take();
    REQUIRE_FALSE(alive.expired());
    bool cleaned = false;
    runner.deferUntilFinished([&] {
        CHECK(alive.expired());
        CHECK(destroyed_on == std::this_thread::get_id());
        CHECK(model.writesPending());
        cleaned = true;
    });
    completion();
    REQUIRE(cleaned);
    REQUIRE(job->state() == (fail ? JobState::Failed : JobState::Done));
}

TEST_CASE("Runner lifecycle observes one occupied publication and one released completion", "[JobRunner][operation]")
{
    OwnerQueue queue;
    ModelLayer model;
    JobRunner runner(model, nullptr, queue.dispatcher());
    const auto owner = std::this_thread::get_id();
    int started = 0, finished = 0;
    runner.setOnStarted([&](Job& job) {
        CHECK(std::this_thread::get_id() == owner);
        CHECK(model.writesPending());
        CHECK(job.masked());
        ++started;
    });
    runner.setOnFinished([&](Job& job) {
        CHECK(std::this_thread::get_id() == owner);
        CHECK_FALSE(model.writesPending());
        CHECK(job.state() == JobState::Done);
        ++finished;
    });
    REQUIRE_FALSE(runner.run("refused", [] { return JobWork { }; }));
    REQUIRE(started == 0);
    auto job = runner.run("accepted", [&] { return systems::job::JobWork { [](ProgressFn) { }, { } }; }, JobOptions { .masked = true });
    REQUIRE(started == 1);
    REQUIRE_THROWS_AS(runner.setOnProgress({ }), std::runtime_error);
    REQUIRE_FALSE(runner.run("busy", [&] { return systems::job::JobWork { [](ProgressFn) { }, { } }; }, { }));
    auto completion = queue.take();
    completion();
    completion();
    REQUIRE(finished == 1);
}

TEST_CASE("Cancellation after commit starts preserves the actual commit outcome", "[JobRunner][operation]")
{
    bool inline_compute = false;
    bool fail = false;
    SECTION("worker computation")
    {
        SECTION("commit succeeds") { }
        SECTION("commit fails") { fail = true; }
    }
    SECTION("inline computation")
    {
        inline_compute = true;
        SECTION("commit succeeds") { }
        SECTION("commit fails") { fail = true; }
    }
    OwnerQueue queue;
    ModelLayer model;
    JobRunner runner(model, nullptr, queue.dispatcher());
    int committed = 0;
    int cleaned = 0;
    int finished = 0;
    const auto expected = fail ? JobState::Failed : JobState::Done;
    runner.setOnFinished([&](Job& job) {
        CHECK(job.state() == expected);
        CHECK(job.isCancellationRequested());
        CHECK_FALSE(model.writesPending());
        ++finished;
    });
    auto job = runner.run("commit cancellation", [&] {
        return JobWork { [](ProgressFn) { }, [&](ProgressFn) {
                            auto current = runner.currentJob();
                            CHECK(current->state() == JobState::Committing);
                            CHECK(model.writesPending());
                            current->cancel();
                            CHECK(current->state() == JobState::Committing);
                            CHECK(runner.deferUntilFinished([&] {
                                CHECK(runner.currentJob()->state() == JobState::Committing);
                                CHECK(model.writesPending());
                                ++cleaned;
                            }));
                            ++committed;
                            if (fail)
                                throw std::runtime_error("commit failed after cancellation"); },
            inline_compute };
    });
    REQUIRE(job);
    if (!inline_compute) {
        REQUIRE(job->state() == JobState::Running);
        auto completion = queue.take();
        completion();
        completion(); // 迟到的相同收尾不重复提交或改变终态。
    }
    REQUIRE(job->state() == expected);
    REQUIRE(job->isCancellationRequested());
    REQUIRE(job->error() == (fail ? "commit failed after cancellation" : ""));
    REQUIRE(committed == 1);
    REQUIRE(cleaned == 1);
    REQUIRE(finished == 1);
    REQUIRE_FALSE(runner.currentJob());
    REQUIRE_FALSE(model.writesPending());
}
