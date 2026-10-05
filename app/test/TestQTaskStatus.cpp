/** @file TestQTaskStatus.cpp
 * @brief 共享 Runner 的展示、取消与后继任务回归。
 */
#include "FeatureHandler.h"
#include "FeatureSystem.h"
#include "JobRunner.h"
#include "MeshData.h"
#include "ModelScope.h"
#include "QTaskStatus.h"
#include "Session.h"
#include "UndoStack.h"
#include "test/OwnerQueue.h"
#include <QCoreApplication>
#include <QTimer>
#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include <thread>
using namespace systems::job;
namespace {
void ensureApplication()
{
    static int argc = 1;
    static char name[] = "TestQTaskStatus";
    static char* argv[] = { name, nullptr };
    static QCoreApplication app(argc, argv);
}
}
TEST_CASE("Shared task status keeps cancellation and busy state until GUI completion", "[QTaskStatus]")
{
    ensureApplication();
    QTaskStatus status;
    OwnerQueue queue;
    session::Session session(nullptr, queue.dispatcher());
    auto& model = session.model();
    auto& runner = *session.jobRunner();
    status.bindSession(session);
    int started = 0, finished = 0, cancelled = 0;
    QObject::connect(&status, &QTaskStatus::taskStarted, [&] { ++started; });
    QObject::connect(&status, &QTaskStatus::taskFinished, [&] { ++finished; });
    QObject::connect(&status, &QTaskStatus::taskCancelled, [&] { ++cancelled; });
    auto job = runner.run("masked", [&] { return systems::job::JobWork { [](ProgressFn report) { report(0.7, "latest"); }, { } }; }, JobOptions { .masked = true });
    auto completion = queue.take();
    REQUIRE(status.isRunning());
    REQUIRE(status.isFrozenBusy());
    REQUIRE(status.isWritePending());
    auto* timer = status.findChild<QTimer*>();
    REQUIRE(timer);
    QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection);
    REQUIRE(status.getProgress() == 0.7);
    REQUIRE(status.getMessage() == "latest");
    REQUIRE_FALSE(runner.run("refused", [&] { return systems::job::JobWork { [](ProgressFn) { }, { } }; }, { }));
    REQUIRE(started == 1);
    REQUIRE(status.getProgress() == 0.7);
    status.cancel();
    REQUIRE(status.getMessage() == QString::fromUtf8("正在取消…"));
    REQUIRE(model.writesPending());
    REQUIRE(status.isWritePending());
    completion();
    REQUIRE(job->state() == JobState::Cancelled);
    REQUIRE_FALSE(model.writesPending());
    REQUIRE_FALSE(status.isRunning());
    REQUIRE_FALSE(status.isFrozenBusy());
    REQUIRE(cancelled == 1);
    REQUIRE(finished == 1);
    completion();
    REQUIRE(finished == 1);
}
TEST_CASE("Masked typed feature keeps progress and GUI events available until writeback", "[QTaskStatus][FeatureSystem]")
{
    ensureApplication();
    class Feature final : public systems::feature::FeatureHandler {
    public:
        Index target;
        std::thread::id worker_thread;
        std::shared_ptr<Job> job;
        std::any execute(systems::feature::FeatureContext& ctx) override
        {
            job = ctx.runTypedWriteback("正式回写", target, [](const ComponentOperator& op) { return op.mesh()->vertex_positions_; }, [this](auto& input, ProgressFn report) {
                    worker_thread = std::this_thread::get_id();
                    report(0.5, "计算中");
                    input.front()[0] *= 2;
                    return std::move(input); }, [](ComponentOperator& op, auto& result, ProgressFn report) {
                    op.editableMesh(MeshEditKind::NonTopology).vertex_positions_ = std::move(result);
                    report(1.0, "安装完成"); }, true);
            return { };
        }
    };
    OwnerQueue queue;
    QTaskStatus status;
    session::Session session(nullptr, queue.dispatcher());
    status.bindSession(session);
    auto& model = session.model();
    auto& undo = session.undoStack();
    Index target;
    {
        ModelScope scope(model, &undo, { }, ModelScope::Kind::Cleanup);
        auto component = std::make_unique<ComponentData>();
        component->mesh = std::make_unique<MeshData>();
        component->mesh->vertex_positions_.push_back({ 1, 2, 3 });
        ComponentDatas components;
        components.push_back(std::move(component));
        const auto model_id = model.addModel("test", std::move(components));
        target = model.modelById(model_id)->componentIds().front();
    }
    auto handler = std::make_unique<Feature>();
    auto* raw = handler.get();
    raw->target = target;
    REQUIRE(session.featureSystem().registerHandler({ "Feature", "正式回写" },
        systems::feature::FeatureSystem::SystemHandlerPtr { handler.release() }));
    session.featureSystem().invoke("Feature");
    auto completion = queue.take();
    REQUIRE(raw->worker_thread != std::this_thread::get_id());
    REQUIRE_FALSE(undo.inOperation());
    REQUIRE(status.isRunning());
    REQUIRE(status.isWritePending());
    REQUIRE(status.isFrozenBusy());
    REQUIRE(model.findComponent(target)->mesh->vertex_positions_.front()[0] == 1);
    bool gui_event = false;
    QTimer::singleShot(0, [&] { gui_event = true; });
    QCoreApplication::processEvents(); // 发布边界已关闭；模拟 GUI 接收事件，未泵提交段。
    REQUIRE(gui_event);
    auto* timer = status.findChild<QTimer*>();
    REQUIRE(timer);
    QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection);
    REQUIRE(status.getProgress() == 0.5);
    REQUIRE(status.getMessage() == QString::fromUtf8("计算中"));
    bool cancelled = false;
    SECTION("success") { }
    SECTION("cancel before GUI commit")
    {
        status.cancel();
        cancelled = true;
        REQUIRE(model.writesPending());
        REQUIRE(status.isFrozenBusy());
    }
    completion();
    REQUIRE_FALSE(status.isRunning());
    REQUIRE_FALSE(status.isFrozenBusy());
    REQUIRE_FALSE(model.writesPending());
    REQUIRE(model.findComponent(target)->mesh->vertex_positions_.front()[0] == (cancelled ? 1 : 2));
    REQUIRE(undo.canUndo() == !cancelled);
    REQUIRE(status.getMessage() == QString::fromUtf8(cancelled ? "已取消" : "安装完成"));
}

TEST_CASE("Shared task status reports inline failures and preserves a successor", "[QTaskStatus]")
{
    ensureApplication();
    QTaskStatus status;
    OwnerQueue queue;
    session::Session session(nullptr, queue.dispatcher());
    auto& runner = *session.jobRunner();
    status.bindSession(session);
    QString error;
    QObject::connect(&status, &QTaskStatus::taskFailed, [&](const QString& value) { error = value; });
    auto failed = runner.run("fail", [&] { return systems::job::JobWork { [](ProgressFn) { throw std::runtime_error("compute failed"); }, { }, true }; }, { });
    REQUIRE(failed->state() == JobState::Failed);
    REQUIRE(error == "compute failed");
    REQUIRE(status.getMessage() == QString::fromUtf8("执行失败：compute failed"));
    REQUIRE_FALSE(status.isRunning());
    std::shared_ptr<Job> successor;
    QObject::connect(&status, &QTaskStatus::taskFinished, [&] {
        if (!successor) {
            REQUIRE(status.getMessage() == "first completed");
            successor = runner.run("successor", [&] { return systems::job::JobWork { [](ProgressFn report) { report(0.4, "next"); }, { } }; }, { });
        }
    });
    auto first = runner.run("first", [&] { return systems::job::JobWork { [](ProgressFn report) { report(1.0, "first completed"); }, { } }; }, { });
    queue.take()();
    REQUIRE(first->state() == JobState::Done);
    REQUIRE(successor);
    REQUIRE(status.isRunning());
    REQUIRE(status.getProgress() == 0.0);
    REQUIRE(status.getMessage().isEmpty());
    queue.take()();
    REQUIRE_FALSE(status.isRunning());
    REQUIRE(status.getProgress() == 1.0);
    REQUIRE(status.getMessage() == "next");
}

TEST_CASE("Short jobs preserve commit feedback and terminal outcomes without a timer tick", "[QTaskStatus][report]")
{
    ensureApplication();
    QTaskStatus status;
    OwnerQueue queue;
    session::Session session(nullptr, queue.dispatcher());
    auto& runner = *session.jobRunner();
    status.bindSession(session);
    bool fail_commit = false, cancel_before_commit = false, cancel_in_commit = false;
    SECTION("success") { }
    SECTION("commit failure overrides success text") { fail_commit = true; }
    SECTION("queued cancellation suppresses write") { cancel_before_commit = true; }
    SECTION("commit reporting does not interrupt an already started write") { cancel_in_commit = true; }
    bool applied = false;
    auto job = runner.run("short", [&] {
        return JobWork { [](ProgressFn report) { report(0.8, "computed"); }, [&](ProgressFn report) {
                            applied = true;
                            if (cancel_in_commit)
                                status.cancel();
                            report(1.0, "installed");
                            if (fail_commit)
                                throw std::runtime_error("install failed"); } };
    });
    auto completion = queue.take();
    REQUIRE(status.getMessage().isEmpty()); // No event pump or timer refresh.
    status.showMessage("asynchronous startup return");
    REQUIRE(status.getMessage().isEmpty());
    if (cancel_before_commit)
        status.cancel();
    completion();
    REQUIRE(applied == !cancel_before_commit);
    const auto expected_state = fail_commit ? JobState::Failed : cancel_before_commit ? JobState::Cancelled
                                                                                      : JobState::Done;
    REQUIRE(job->state() == expected_state);
    REQUIRE_FALSE(status.isRunning());
    REQUIRE_FALSE(session.model().writesPending());
    const auto expected_message = fail_commit ? QString::fromUtf8("执行失败：install failed") : cancel_before_commit ? QString::fromUtf8("已取消")
                                                                                                                     : QString("installed");
    REQUIRE(status.getMessage() == expected_message);
    // A late timer tick cannot replace the terminal feedback.
    QMetaObject::invokeMethod(status.findChild<QTimer*>(), "timeout", Qt::DirectConnection);
    REQUIRE(status.getMessage() == expected_message);
}

TEST_CASE("Synchronous UI feedback shares the status text without creating a job", "[QTaskStatus][report]")
{
    ensureApplication();
    QTaskStatus status;
    REQUIRE(QMetaObject::invokeMethod(&status, "showMessage", Qt::DirectConnection, Q_ARG(QString, "select a face")));
    REQUIRE(status.getMessage() == "select a face");
    REQUIRE(QMetaObject::invokeMethod(&status, "reportFailure", Qt::DirectConnection, Q_ARG(QString, "invalid input")));
    REQUIRE(status.getMessage() == QString::fromUtf8("执行失败：invalid input"));
    status.showMessage({ });
    REQUIRE(status.getMessage() == QString::fromUtf8("执行失败：invalid input"));
    REQUIRE_FALSE(status.isRunning());
    REQUIRE_FALSE(status.isWritePending());
}

TEST_CASE("Successful jobs without feedback show completion and clear the previous message", "[QTaskStatus][report]")
{
    ensureApplication();
    QTaskStatus status;
    OwnerQueue queue;
    session::Session session(nullptr, queue.dispatcher());
    status.bindSession(session);
    status.showMessage("old result");
    auto job = session.jobRunner()->run("quiet", [] { return JobWork { [](ProgressFn) { } }; });
    REQUIRE(status.getMessage().isEmpty());
    queue.take()();
    REQUIRE(job->state() == JobState::Done);
    REQUIRE(status.getMessage() == QString::fromUtf8("已完成"));
}
