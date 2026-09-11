/**
 * @file test_runtime.cpp
 * @brief precess::Runtime 内嵌宿主测试：初始化、控制台执行与活会话调用
 *
 * 单进程只构造一个 Runtime 实例且全用例线性推进（Catch2 SECTION 会重跑
 * 用例装配，解释器 Initialize/Finalize 循环重入叠加 pybind11 内部状态不可
 * 靠）；插件目录缺失时跳过会话调用段。
 */
#include "precess/runtime.h"

#include "Session.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

using precess::Runtime;

TEST_CASE("precess Runtime hosts embedded interpreter", "[python][runtime]")
{
    session::Session session;

    Runtime::Config config;
    config.python_home = PRECESS_PYTHON_HOME;
    config.module_dirs = { PRECESS_PYTHON_MODULE_DIR };
    Runtime runtime(&session, std::move(config));

    // —— 懒初始化：幂等，成功后 precess 模块可用 ——
    CHECK_FALSE(runtime.isAvailable());
    runtime.initialize();
    REQUIRE(runtime.isAvailable());
    CHECK(runtime.lastError().empty());
    runtime.initialize(); // 重复调用无副作用
    CHECK(runtime.isAvailable());

    // —— 表达式执行与输出捕获 ——
    Runtime::ExecutionResult result = runtime.execute("1 + 1");
    REQUIRE(result.ok);
    CHECK(result.output.find("2") != std::string::npos);

    // 变量跨执行存活（共享 __main__ 命名空间）
    runtime.execute("answer = 6 * 7");
    result = runtime.execute("answer");
    REQUIRE(result.ok);
    CHECK(result.output.find("42") != std::string::npos);

    // 运行期错误以 traceback 回显，不终止宿主
    result = runtime.execute("1 / 0");
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("ZeroDivisionError") != std::string::npos);
    result = runtime.execute("1 + 1");
    REQUIRE(result.ok);

    // —— 续行判定：未完块只编译不执行（续行缓冲由调用方拼接）——
    std::string buffer = "def f():\n";
    result = runtime.execute(buffer);
    CHECK(result.incomplete);
    CHECK_FALSE(result.ok);
    buffer += "    return 21 * 2\n";
    result = runtime.execute(buffer);
    REQUIRE(result.ok);
    result = runtime.execute("f()");
    REQUIRE(result.ok);
    CHECK(result.output.find("42") != std::string::npos);

    // —— 活会话调用：precess.current 即构造时注入的 session ——
    const std::filesystem::path plugin_dir(PRECESS_PLUGIN_DIR);
    if (std::filesystem::is_directory(plugin_dir)) {
        session.loadStaticPlugins();
        session.loadPluginsFromDirectory(plugin_dir);

        // 控制台经活会话调用功能：写入目标 = 新建 Model（Combo 选项下标 2）。
        // single 模式一次执行一条语句，两条语句分两次执行（与 REPL 一致）
        result = runtime.execute("import precess");
        REQUIRE(result.ok);
        result = runtime.execute("precess.current.call('CreateBox', 0, 0, 0, 5, 5, 5, 2)");
        if (!result.ok)
            FAIL(result.error);
        REQUIRE(result.ok);

        // 同一会话在 C++ 侧可见：新建模型承载一个盒体组件（invoke 是操作边界）
        const auto models = session.query().listModels();
        REQUIRE(models.size() == 1);
        const auto components = session.query().componentSummaries(models[0].model_id);
        REQUIRE(components.size() == 1);
        const auto summary = session.query().geometrySummary(components[0].component_id);
        CHECK(summary.has_geometry);
        CHECK(summary.face_count == 6);
        CHECK(summary.vertex_count == 8);
    } else {
        WARN("plugin dir not found, skip session e2e");
    }

    // 作用域结束：先丢弃 precess.current 再终结解释器，随后会话正常析构
}
