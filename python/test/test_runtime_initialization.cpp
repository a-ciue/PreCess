/** @brief 初始化失败测试。CTest 为每个用例启动独立进程，不重复初始化解释器。 */
#include <pybind11/embed.h>
#include "python/runtime.h"
#include "Session.h"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace {
// 仅本测试线程可见；生产 Runtime 不编译故障检查点。
thread_local std::string_view failure_stage;
thread_local unsigned int failure_hits = 0;
}

namespace python {
void runtimeInitializationCheckpoint(const char* stage)
{
    if (failure_stage != stage)
        return;
    ++failure_hits;
    if (failure_stage == "import") {
        // 让真正的 import precess 抛出 Python 异常，不依赖机器上的模块搜索路径。
        pybind11::exec(R"PY(
import sys
class RejectPrecess:
    def find_spec(self, fullname, path=None, target=None):
        if fullname == 'precess':
            raise ImportError('injected precess import failure')
sys.meta_path.insert(0, RejectPrecess())
)PY");
    } else if (failure_stage == "before_release") {
        throw std::bad_alloc(); // 模拟守卫分配前失败，不耗尽真实内存。
    } else {
        throw std::runtime_error("injected initialization tail failure");
    }
}
}

namespace {
void checkRollback(std::string_view stage)
{
    std::jthread watchdog([](std::stop_token stop) {
        const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(15);
        while (!stop.stop_requested()) {
            if (std::chrono::steady_clock::now() >= limit)
                std::abort();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    failure_stage = stage;
    failure_hits = 0;
    session::Session session;
    python::Runtime::Config config;
    config.python_home = PRECESS_PYTHON_HOME;
    config.module_dirs = { PRECESS_PYTHON_MODULE_DIR };
    {
        python::Runtime runtime(&session, std::move(config));
        runtime.initialize();
        REQUIRE(failure_hits == 1);
        CHECK_FALSE(runtime.isAvailable());
        REQUIRE_FALSE(runtime.lastError().empty());
        const auto error = runtime.lastError();
        if (stage == "import")
            CHECK(error.find("injected precess import failure") != std::string::npos);
        CHECK(Py_IsInitialized() == 0);
        runtime.initialize();
        CHECK(failure_hits == 1);
        CHECK_FALSE(runtime.isAvailable());
        CHECK(runtime.lastError() == error);
        CHECK(Py_IsInitialized() == 0);
        const auto result = runtime.execute("1 + 1");
        CHECK_FALSE(result.ok);
        CHECK(result.error == error);
        CHECK(failure_hits == 1);
    }
    CHECK(Py_IsInitialized() == 0);
}
}

TEST_CASE("Runtime rolls back failed precess import", "[python][initialization]")
{
    checkRollback("import");
}
TEST_CASE("Runtime rolls back console C++ exception", "[python][initialization]")
{
    checkRollback("console");
}
TEST_CASE("Runtime rolls back GIL guard allocation failure", "[python][initialization]")
{
    checkRollback("before_release");
}
TEST_CASE("Runtime restores GIL before initialization rollback", "[python][initialization]")
{
    checkRollback("after_release");
}
TEST_CASE("Runtime does not finalize an externally owned interpreter", "[python][initialization]")
{
    PyConfig config;
    PyConfig_InitPythonConfig(&config);
    const auto status = PyConfig_SetString(&config, &config.home,
        std::filesystem::path(PRECESS_PYTHON_HOME).wstring().c_str());
    if (PyStatus_Exception(status)) {
        PyConfig_Clear(&config);
        FAIL("Could not configure external interpreter Python home");
    }
    // pybind11 负责清理传入的 PyConfig，与 Runtime 使用相同标准库。
    pybind11::scoped_interpreter external(&config, 0, nullptr, false);
    {
        python::Runtime runtime(nullptr, {});
        runtime.initialize();
        CHECK_FALSE(runtime.isAvailable());
        CHECK(runtime.lastError().find("exclusive ownership") != std::string::npos);
        runtime.initialize();
        CHECK(Py_IsInitialized() != 0);
    }
    CHECK(Py_IsInitialized() != 0);
    CHECK(pybind11::eval("1 + 1").cast<int>() == 2);
}
