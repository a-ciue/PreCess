/** @brief 宿主空闲期后台线程回归；观察结果与清理阶段严格分开。 */
#include "Session.h"
#include "python/runtime.h"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <pybind11/embed.h>
#include <thread>

TEST_CASE("embedded Python worker progresses while host is idle", "[python][threads]")
{
    namespace py = pybind11;
    // 独立测试进程的兜底：包含初始化、清理及解释器析构，避免永久挂起。
    std::jthread watchdog([](std::stop_token stop) {
        const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(15);
        while (!stop.stop_requested()) {
            if (std::chrono::steady_clock::now() >= limit)
                std::abort();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    std::atomic<bool> gate { false }, finished { false }, succeeded { false };
    session::Session session;
    python::Runtime::Config config;
    config.python_home = PRECESS_PYTHON_HOME;
    config.module_dirs = { PRECESS_PYTHON_MODULE_DIR };
    python::Runtime runtime(&session, std::move(config));
    runtime.initialize();
    REQUIRE(runtime.isAvailable());
    {
        py::gil_scoped_acquire gil;
        auto globals = py::module_::import("__main__").attr("__dict__");
        globals["_gate"] = py::cpp_function([&] { return gate.load(); });
        globals["_finish"] = py::cpp_function([&](bool ok) {
            succeeded = ok;
            finished = true;
        });
    }
    const auto start = runtime.execute(R"PY(exec('''
import asyncio, threading
def _worker():
    while not _gate():
        pass
    try:
        loop = asyncio.new_event_loop()
        loop.close()
        assert sum(range(10000)) == 49995000
        _finish(True)
    except BaseException:
        _finish(False)
_thread = threading.Thread(target=_worker, daemon=True)
_thread.start()
'''))PY");
    if (!start.ok) {
        std::fprintf(stderr, "Worker startup failed; refusing unsafe Runtime teardown: %s\n", start.error.c_str());
        std::fflush(stderr);
        std::_Exit(EXIT_FAILURE);
    }
    gate = true;
    // 观察窗口内只使用原生等待，不调用 Python，不主动释放 GIL。
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!finished && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    const bool completed_while_idle = finished.load();
    // 先固定结果，再允许清理阶段释放 GIL；清理完成不能把失败改成成功。
    const auto cleanup = runtime.execute("_thread.join(timeout=2)");
    CHECK(cleanup.ok);
    CHECK(completed_while_idle);
    CHECK(succeeded.load());
    const auto stopped = runtime.execute("assert not _thread.is_alive()");
    if (!cleanup.ok || !stopped.ok) {
        std::fprintf(stderr, "Worker did not safely stop; refusing Runtime teardown: %s %s\n",
            cleanup.error.c_str(), stopped.error.c_str());
        std::fflush(stderr);
        std::_Exit(EXIT_FAILURE);
    }
}
