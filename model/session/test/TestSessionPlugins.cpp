/**
 * @file TestSessionPlugins.cpp
 * @brief Session 插件装载冒烟测试：动态插件注册与功能信息（Qt 插件机制，需 QCoreApplication）
 *
 * 插件目录经编译期宏 PRECESS_PLUGIN_DIR 注入（构建树 plugins 目录），
 * 用以验证 Session 的系统装配与插件注册链路端到端可用。
 */
#include "AlgorithmSystem.h"
#include "Session.h"

#include "FeatureInfo.h"
#include "FeatureSystem.h"
#include "JobRunner.h"
#include "ModelIOSystem.h"
#include "SystemPluginManager.h"
#include "test/OwnerQueue.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
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
    // 使用默认构建的产品插件，不能依赖 PRECESS_BUILD_EXAMPLES=ON。
    SECTION("feature") { plugin_fragment = "CreatePointPlugin"; }
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

TEST_CASE("Binary algorithm plugins accept sidecar navigation without losing legacy registration", "[session][plugins][navigation]")
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);
#ifdef PRECESS_PLUGIN_DIR
    const std::filesystem::path plugin_dir(PRECESS_PLUGIN_DIR);
#else
    SKIP("PRECESS_PLUGIN_DIR not defined");
#endif
    std::filesystem::path source;
    for (const auto& entry : std::filesystem::directory_iterator(plugin_dir)) {
        if (entry.path().stem().string().find("CmdExecutePlugin") != std::string::npos
            && (entry.path().extension() == ".dll" || entry.path().extension() == ".so" || entry.path().extension() == ".dylib")) {
            source = entry.path();
            break;
        }
    }
    REQUIRE_FALSE(source.empty());
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto target = std::filesystem::path(directory.path().toStdString()) / source.filename();
    REQUIRE(QFile::copy(QString::fromStdString(source.string()), QString::fromStdString(target.string())));
    auto declaration_path = target;
    declaration_path.replace_extension(".navigation.json");
    QFile declaration(QString::fromStdString(declaration_path.string()));
    REQUIRE(declaration.open(QIODevice::WriteOnly));
    bool expected_navigation = false;
    bool expected_custom_category = false;
    SECTION("Valid declarations supplement a previously unclassified binary")
    {
        REQUIRE(declaration.write(R"({"categories":["tetrahedron"],"group":"Generate","order":10,"label":"Mesh generation"})") > 0);
        expected_navigation = true;
    }
    SECTION("Custom category objects reach the native registry through a binary sidecar")
    {
        REQUIRE(declaration.write(R"({"categories":[{"id":"custom","title":"Custom meshing","order":-5}],"group":"Generate","order":10,"label":"Mesh generation"})") > 0);
        expected_navigation = true;
        expected_custom_category = true;
    }
    SECTION("Malformed declarations preserve plugin availability")
    {
        REQUIRE(declaration.write("{invalid") > 0);
    }
    SECTION("Non-object declarations preserve plugin availability")
    {
        REQUIRE(declaration.write("[]") > 0);
    }
    declaration.close();
    Session session;
    REQUIRE(session.pluginManager().registerPlugin(target));
    const auto infos = session.algorithmSystem().getAlgorithmInfos();
    REQUIRE(infos.size() == 1);
    CHECK(infos.front()->name == "cmdExecutePlugin");
    if (expected_navigation) {
        CHECK(infos.front()->navigation.categories == std::vector<std::string> { expected_custom_category ? "custom" : "tetrahedron" });
        const auto categories = session.algorithmSystem().getNavigationCategories();
        REQUIRE(categories.size() == 1);
        if (expected_custom_category) {
            CHECK(categories[0].title == "Custom meshing");
            CHECK(categories[0].order == -5);
        }
        CHECK(infos.front()->navigation.group == "Generate");
        CHECK(infos.front()->navigation.order == 10);
        CHECK(infos.front()->navigation.label == "Mesh generation");
    } else {
        CHECK(infos.front()->navigation.categories.empty());
        CHECK(infos.front()->navigation.label.empty());
    }
    declaration.close();
    session.pluginManager().unregisterPlugin(target);
    REQUIRE(session.pluginManager().registerPlugin(target));
    CHECK(session.algorithmSystem().getAlgorithmInfos().size() == 1);
    session.pluginManager().unregisterPlugin(target);
    CHECK(session.algorithmSystem().getAlgorithmInfos().empty());
    CHECK(session.algorithmSystem().getNavigationCategories().empty());
}
