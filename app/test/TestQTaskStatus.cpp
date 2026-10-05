/** @file TestQTaskStatus.cpp
 * @brief 共享 Runner 的展示、取消与后继任务回归。
 */
#include "JobRunner.h"
#include "QTaskStatus.h"
#include "Session.h"
#include "test/OwnerQueue.h"
#include <QCoreApplication>
#include <QTimer>
#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
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
    REQUIRE(status.getProgressLabel() == "latest");
    REQUIRE_FALSE(runner.run("refused", [&] { return systems::job::JobWork { [](ProgressFn) { }, { } }; }, { }));
    REQUIRE(started == 1);
    REQUIRE(status.getProgress() == 0.7);
    status.cancel();
    REQUIRE(status.getProgressLabel() == QString::fromUtf8("正在取消…"));
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
    REQUIRE_FALSE(status.isRunning());
    std::shared_ptr<Job> successor;
    QObject::connect(&status, &QTaskStatus::taskFinished, [&] {
        if (!successor)
            successor = runner.run("successor", [&] { return systems::job::JobWork { [](ProgressFn report) { report(0.4, "next"); }, { } }; }, { });
    });
    auto first = runner.run("first", [&] { return systems::job::JobWork { [](ProgressFn) { }, { } }; }, { });
    queue.take()();
    REQUIRE(first->state() == JobState::Done);
    REQUIRE(successor);
    REQUIRE(status.isRunning());
    REQUIRE(status.getProgress() == 0.0);
    queue.take()();
    REQUIRE_FALSE(status.isRunning());
    REQUIRE(status.getProgress() == 1.0);
}
