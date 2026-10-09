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

TEST_CASE("Algorithm plugins ignore obsolete navigation sidecars", "[session][plugins][navigation]")
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
    SECTION("Former navigation declarations are ignored")
    {
        REQUIRE(declaration.write(R"({"categories":[{"id":"custom","title":"Custom meshing"}],"label":"Ignored"})") > 0);
    }
    SECTION("Malformed files are not read")
    {
        REQUIRE(declaration.write("{invalid") > 0);
    }
    declaration.close();
    Session session;
    REQUIRE(session.pluginManager().registerPlugin(target));
    const auto infos = session.algorithmSystem().getAlgorithmInfos();
    REQUIRE(infos.size() == 1);
    CHECK(infos.front()->name == "cmdExecutePlugin");
    declaration.close();
    session.pluginManager().unregisterPlugin(target);
    REQUIRE(session.pluginManager().registerPlugin(target));
    CHECK(session.algorithmSystem().getAlgorithmInfos().size() == 1);
    session.pluginManager().unregisterPlugin(target);
    CHECK(session.algorithmSystem().getAlgorithmInfos().empty());
}

TEST_CASE("Binary feature setup supplies navigation and survives reload", "[session][plugins][setup][navigation]")
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);
    const std::filesystem::path source(PRECESS_SETUP_TEST_PLUGIN);
    REQUIRE(std::filesystem::exists(source));
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto target = std::filesystem::path(directory.path().toStdString()) / source.filename();
    REQUIRE(QFile::copy(QString::fromStdString(source.string()), QString::fromStdString(target.string())));
    SECTION("Minimal identity JSON") { }
    SECTION("Conflicting legacy sidecar cannot override setup")
    {
        auto sidecar = target;
        sidecar.replace_extension(".navigation.json");
        QFile declaration(QString::fromStdString(sidecar.string()));
        REQUIRE(declaration.open(QIODevice::WriteOnly));
        REQUIRE(declaration.write(R"({"categories":["triangle"],"label":"Legacy label"})") > 0);
    }
    Session session;
    for (int reload = 0; reload < 2; ++reload) {
        REQUIRE(session.pluginManager().registerPlugin(target));
        const auto infos = session.featureSystem().getFeatureInfos();
        REQUIRE(infos.size() == 1);
        CHECK(infos.front()->name == "setupNavigationTest");
        CHECK(infos.front()->navigation.label() == "Setup feature");
        CHECK(infos.front()->navigation.categories() == std::vector<std::string> { "setup-custom" });
        CHECK(infos.front()->navigation.order() == 7);
        CHECK(infos.front()->navigation.categoryDefaults().at("setup-custom").at("Size") == "3");
        REQUIRE(infos.front()->arg_types.size() == 1);
        CHECK(infos.front()->arg_types.front().name == "Size");
        const auto categories = session.featureSystem().getNavigationCategories();
        REQUIRE(categories.size() == 1);
        CHECK(categories.front().title == "Setup custom category");
        CHECK(categories.front().order == 5);
        CHECK(categories.front().menu_path == "Tools/Custom");
        session.pluginManager().unregisterPlugin(target);
        CHECK(session.featureSystem().getFeatureInfos().empty());
        CHECK(session.featureSystem().getNavigationCategories().empty());
    }
}
