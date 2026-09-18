/**
 * @file TestQPythonRuntime.cpp
 * @brief QPythonRuntime 主线程投递测试：precess_app.call_later 的延时回调经
 *        QTimer 回到 GUI 线程执行
 *
 * 单进程只构造一个 QPythonRuntime（内嵌解释器 Initialize/Finalize 循环重入
 * 不可靠，同 TestPrecessRuntime 约束）；Python 嵌入不可用（桩实现，available
 * 恒 false）时整用例跳过——"不可用"是运行时状态，不算失败。
 */
#include "QPythonRuntime.h"

#include "Session.h"

#include <QCoreApplication>
#include <QThread>

#include <catch2/catch_test_macros.hpp>

namespace {

//! @brief 进程内唯一的 QCoreApplication：Catch2 单进程不可重复构造，静态化以兼容后续用例
QCoreApplication& ensureApplication()
{
    static int argc = 1;
    static char app_name[] = "TestQPythonRuntime";
    static char* argv[] = { app_name, nullptr };
    static QCoreApplication app(argc, argv);
    return app;
}

} // namespace

TEST_CASE("QPythonRuntime dispatches deferred callbacks on the GUI thread")
{
    (void)ensureApplication();

    session::Session session;
    QPythonRuntime runtime(&session);
    runtime.initialize();
    if (!runtime.isAvailable()) {
        WARN("Python runtime unavailable, skip: " << runtime.lastError().toStdString());
        return;
    }

    QVariantMap result = runtime.execute("import precess_app");
    REQUIRE(result["ok"].toBool());

    // 回调记录执行线程是否解释器主线程（初始化线程 ≡ GUI 线程）并置位标志
    result = runtime.execute("import threading");
    REQUIRE(result["ok"].toBool());
    result = runtime.execute(
        "def _cb():\n"
        "    global fired, on_main\n"
        "    fired = True\n"
        "    on_main = threading.current_thread() is threading.main_thread()\n");
    REQUIRE(result["ok"].toBool());
    result = runtime.execute("fired = False");
    REQUIRE(result["ok"].toBool());
    result = runtime.execute("on_main = False");
    REQUIRE(result["ok"].toBool());

    // 1ms 延时投递：app 注册的 precess_app.call_later 经 QTimer 排入事件循环
    result = runtime.execute("precess_app.call_later(1, _cb)");
    INFO(result["error"].toString().toStdString());
    REQUIRE(result["ok"].toBool());

    // 抽干事件循环直至回调执行（延时未到时 processEvents 不触发定时器，轮询等待）
    bool fired = false;
    for (int i = 0; i < 500 && !fired; ++i) {
        QCoreApplication::processEvents();
        const QVariantMap probe = runtime.execute("fired");
        fired = probe["ok"].toBool() && probe["output"].toString().contains("True");
        if (!fired)
            QThread::msleep(10);
    }
    REQUIRE(fired);

    const QVariantMap thread_probe = runtime.execute("on_main");
    REQUIRE(thread_probe["ok"].toBool());
    CHECK(thread_probe["output"].toString().contains("True"));

    // 任意签名通路：多参 + 默认值 + std::string 返回（pybind11 类型转换器）
    result = runtime.execute("precess_app.echo('ab', 3)");
    INFO(result["error"].toString().toStdString());
    REQUIRE(result["ok"].toBool());
    CHECK(result["output"].toString().contains("ababab"));
    result = runtime.execute("precess_app.echo('x')");
    REQUIRE(result["ok"].toBool());
    CHECK(result["output"].toString().contains("x"));

    // 回调异常在包内消化（记日志），事件循环与解释器不受影响
    result = runtime.execute("precess_app.call_later(0, lambda: 1 / 0)");
    REQUIRE(result["ok"].toBool());
    for (int i = 0; i < 50; ++i) {
        QCoreApplication::processEvents();
        QThread::msleep(10);
    }
    const QVariantMap alive = runtime.execute("1 + 1");
    REQUIRE(alive["ok"].toBool());
}
