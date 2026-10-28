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
#include <QLibrary>
#include <QPluginLoader>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

using namespace session;

namespace {
int static_standard_attempts = 0;
int static_unknown_attempts = 0;
QObject* failStaticStandardConstruction()
{
    ++static_standard_attempts;
    throw std::runtime_error("expected static plugin construction failure");
}
QObject* failStaticUnknownConstruction()
{
    ++static_unknown_attempts;
    throw 42;
}
}

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
            for (const auto& menu : info->navigation.entries()) {
                if (menu.menu_id == "geometry")
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
        CHECK(infos.front()->navigation.entryIds() == std::vector<std::string> { "setup-custom" });
        CHECK(infos.front()->navigation.order() == 7);
        CHECK(infos.front()->navigation.entryDefaults().at("setup-custom").at("Size") == "3");
        REQUIRE(infos.front()->arg_types.size() == 1);
        CHECK(infos.front()->arg_types.front().name == "Size");
        const auto categories = session.featureSystem().getNavigationEntries();
        REQUIRE(categories.size() == 1);
        CHECK(categories.front().title == "Setup custom category");
        CHECK(categories.front().order == 5);
        CHECK(categories.front().menu_id == "setup-tools");
        CHECK(categories.front().group_id == "custom");
        const auto menus = session.featureSystem().getNavigationMenus();
        REQUIRE(menus.size() == 1);
        CHECK(menus.front().id == "setup-tools");
        CHECK(menus.front().title == "Tools");
        const auto groups = session.featureSystem().getNavigationGroups();
        REQUIRE(groups.size() == 1);
        CHECK(groups.front().menu_id == "setup-tools");
        CHECK(groups.front().id == "custom");
        CHECK(groups.front().title == "Custom");
        session.pluginManager().unregisterPlugin(target);
        CHECK(session.featureSystem().getFeatureInfos().empty());
        CHECK(session.featureSystem().getNavigationEntries().empty());
        CHECK(session.featureSystem().getNavigationMenus() == menus);
        CHECK(session.featureSystem().getNavigationGroups() == groups);
    }
}

TEST_CASE("Directory plugin loading continues after registration exceptions and permits retry", "[session][plugins][failure]")
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);
    const std::filesystem::path source(PRECESS_SETUP_TEST_PLUGIN);
    REQUIRE(std::filesystem::exists(source));
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto plugin_dir = std::filesystem::path(directory.path().toStdString());
    const auto first = plugin_dir / ("first" + source.extension().string());
    const auto second = plugin_dir / ("second" + source.extension().string());
    for (const auto& target : { first, second })
        REQUIRE(QFile::copy(QString::fromStdString(source.string()), QString::fromStdString(target.string())));

    class FailingOnceRegister : public systems::SystemRegisterBase {
    public:
        bool unknown_exception = false;
        int attempts = 0;
        bool registerPlugin(const QJsonObject&, systems::PluginBase&) override
        {
            if (++attempts == 1) {
                if (unknown_exception)
                    throw 42;
                throw std::runtime_error("expected registration failure");
            }
            return true;
        }
        void unregisterPlugin(const QJsonObject&) override { }
    };
    auto registrar = std::make_unique<FailingOnceRegister>();
    auto* probe = registrar.get();
    SECTION("standard exception") { }
    SECTION("unknown exception") { probe->unknown_exception = true; }
    Session session;
    auto& manager = session.pluginManager();
    manager.removeSystemRegister(systems::feature::FeatureSystem::name);
    REQUIRE(manager.addSystemRegister(systems::feature::FeatureSystem::name, std::move(registrar)));
    REQUIRE_NOTHROW(session.loadPluginsFromDirectory(plugin_dir));
    CHECK(probe->attempts == 2);
    const auto names = manager.getPluginNames();
    REQUIRE(names.size() == 1);
    const auto& rejected = names.front() == first.stem().string() ? second : first;
    REQUIRE(manager.registerPlugin(rejected));
    CHECK(probe->attempts == 3);
    CHECK(manager.getPluginNames().size() == 2);
}

TEST_CASE("Dynamic plugin construction exceptions return false and permit retry", "[session][plugins][failure]")
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);
    const std::filesystem::path source(PRECESS_SETUP_TEST_PLUGIN);
    for (const auto& failure : { QByteArray("standard"), QByteArray("unknown") }) {
        QTemporaryDir directory;
        REQUIRE(directory.isValid());
        const auto target = std::filesystem::path(directory.path().toStdString()) / source.filename();
        REQUIRE(QFile::copy(QString::fromStdString(source.string()), QString::fromStdString(target.string())));
        const auto previous = qgetenv("PRECESS_TEST_SETUP_CONSTRUCTION");
        const auto restore = qScopeGuard([previous] {
            if (previous.isNull())
                qunsetenv("PRECESS_TEST_SETUP_CONSTRUCTION");
            else
                qputenv("PRECESS_TEST_SETUP_CONSTRUCTION", previous);
        });
        Session session;
        qputenv("PRECESS_TEST_SETUP_CONSTRUCTION", failure);
        REQUIRE_FALSE(session.pluginManager().registerPlugin(target));
        CHECK(session.pluginManager().getPluginNames().empty());
        CHECK(session.featureSystem().getFeatureInfos().empty());
        qunsetenv("PRECESS_TEST_SETUP_CONSTRUCTION");
        REQUIRE(session.pluginManager().registerPlugin(target));
        CHECK(session.featureSystem().getFeatureInfos().size() == 1);
    }
}

TEST_CASE("Static plugin construction exceptions do not stop later plugins", "[session][plugins][failure]")
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);
    QLibrary library(QString::fromUtf8(PRECESS_SETUP_TEST_PLUGIN));
    library.setLoadHints(QLibrary::PreventUnloadHint);
    REQUIRE(library.load());
    const auto metadata = reinterpret_cast<QtPluginMetaDataFunction>(library.resolve("qt_plugin_query_metadata_v2"));
    const auto instance = reinterpret_cast<QtPluginInstanceFunction>(library.resolve("qt_plugin_instance"));
    REQUIRE(metadata);
    REQUIRE(instance);
    static_standard_attempts = 0;
    static_unknown_attempts = 0;
    // 工厂与元数据来自同一测试 DLL，PreventUnload 保证全局静态注册表中的指针有效。
    qRegisterStaticPluginFunction(QStaticPlugin(failStaticStandardConstruction, metadata));
    qRegisterStaticPluginFunction(QStaticPlugin(failStaticUnknownConstruction, metadata));
    qRegisterStaticPluginFunction(QStaticPlugin(instance, metadata));
    Session session;
    REQUIRE_NOTHROW(session.loadStaticPlugins());
    CHECK(static_standard_attempts == 1);
    CHECK(static_unknown_attempts == 1);
    CHECK(session.pluginManager().getPluginNames().size() == 1);
    CHECK(session.featureSystem().getFeatureInfos().size() == 1);
}
