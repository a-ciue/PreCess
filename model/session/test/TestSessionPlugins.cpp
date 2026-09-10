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
#include "SystemPluginManager.h"

#include <QCoreApplication>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>

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
