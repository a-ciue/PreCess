/**
 * @file TestSessionPlugins.cpp
 * @brief Session 插件装载冒烟测试：动态插件注册与功能信息（Qt 插件机制，需 QCoreApplication）
 *
 * 插件目录经编译期宏 PRECESS_PLUGIN_DIR 注入（构建树 plugins 目录），
 * 用以验证 Session 的系统装配与插件注册链路端到端可用。
 */
#include "Session.h"

#include "FeatureInfo.h"
#include "FeatureSystem.h"
#include "JobRunner.h"
#include "ModelIOSystem.h"
#include "SystemPluginManager.h"
#include "test/OwnerQueue.h"

#include <QCoreApplication>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

using namespace session;

TEST_CASE("Session loads plugins and registers feature infos", "[session][plugins]")
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);

#ifdef PRECESS_PLUGIN_DIR
    const std::filesystem::path plugin_dir(PRECESS_PLUGIN_DIR);
#else
    SKIP("PRECESS_PLUGIN_DIR not defined");
#endif

    Session s;
    s.loadStaticPlugins();
    s.loadPluginsFromDirectory(plugin_dir);

    SECTION("动态插件注册进对应系统")
    {
        const auto plugin_names = s.pluginManager().getPluginNames();
        CHECK(plugin_names.size() >= 12);
    }

    SECTION("功能插件经声明链暴露菜单贡献")
    {
        const auto& infos = s.featureSystem().getFeatureInfos();
        CHECK(infos.size() >= 12);
        bool has_geometry_menu = false;
        for (const systems::feature::FeatureInfo* info : infos) {
            for (const auto& menu : info->menus) {
                if (menu.menu_path == "几何")
                    has_geometry_menu = true;
            }
        }
        CHECK(has_geometry_menu);
    }
}

TEST_CASE("Busy plugin unload keeps feature or IO registration until a successful retry", "[session][plugins][job]")
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);
#ifdef PRECESS_PLUGIN_DIR
    const std::filesystem::path plugin_dir(PRECESS_PLUGIN_DIR);
#else
    SKIP("PRECESS_PLUGIN_DIR not defined");
#endif
    std::string plugin_fragment;
    SECTION("feature") { plugin_fragment = "FeatureDemoPlugin"; }
    SECTION("IO") { plugin_fragment = "OffModelPlugin"; }
    std::filesystem::path plugin_path;
    for (const auto& entry : std::filesystem::directory_iterator(plugin_dir)) {
        const auto path = entry.path();
        const auto extension = path.extension().string();
        if (path.stem().string().find(plugin_fragment) != std::string::npos
            && (extension == ".dll" || extension == ".so" || extension == ".dylib")) {
            plugin_path = path;
            break;
        }
    }
    REQUIRE_FALSE(plugin_path.empty());
    OwnerQueue queue;
    Session session(nullptr, queue.dispatcher());
    REQUIRE(session.pluginManager().registerPlugin(plugin_path));
    const auto names = session.pluginManager().getPluginNames();
    const auto infos = session.featureSystem().getFeatureInfos();
    const auto io_infos = session.ioSystem().registeredFileTypeInfos();
    REQUIRE(names.size() == 1);
    REQUIRE(infos.size() + io_infos.size() == 1);
    auto job = session.jobRunner()->run("pure", [] {
        return systems::job::JobWork { [](systems::job::ProgressFn) { } };
    });
    REQUIRE(job);
    auto complete = queue.take();
    REQUIRE_THROWS_AS(session.pluginManager().unregisterPlugin(plugin_path), ModelOperationBusy);
    REQUIRE(session.pluginManager().getPluginNames() == names);
    REQUIRE(session.featureSystem().getFeatureInfos() == infos);
    REQUIRE(session.ioSystem().registeredFileTypeInfos() == io_infos);
    REQUIRE_FALSE(job->isCancellationRequested());
    complete();
    REQUIRE(job->state() == systems::job::JobState::Done);
    REQUIRE_NOTHROW(session.pluginManager().unregisterPlugin(plugin_path));
    REQUIRE(session.pluginManager().getPluginNames().empty());
    REQUIRE(session.featureSystem().getFeatureInfos().empty());
    REQUIRE(session.ioSystem().registeredFileTypeInfos().empty());
}
