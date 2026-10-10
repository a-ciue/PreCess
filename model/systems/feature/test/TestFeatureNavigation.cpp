/** @file TestFeatureNavigation.cpp
 * @brief Feature 的通用分类聚合及持久参数回归测试。
 */
#include "ArgObject.h"
#include "FeatureHandler.h"
#include "FeatureRegistrar.h"
#include "FeatureSystem.h"
#include "ModelLayer.h"
#include <catch2/catch_test_macros.hpp>
#include <type_traits>
#include <utility>

using namespace systems::feature;
namespace {
class Mesher : public FeatureHandler {
public:
    std::vector<FeatureNavigationEntry> categories;
    std::string default_size = "1";
    int* setup_calls = nullptr;
    void setup(FeatureRegistrar& registrar, FeatureContext&) override
    {
        if (setup_calls)
            ++*setup_calls;
        registrar.addParameter({ ArgTypeEnum::Float, "size", default_size });
        for (const auto& category : categories)
            registrar.navigation().addEntry(category);
    }
};
FeatureSystem::SystemHandlerPtr mesher(std::vector<FeatureNavigationEntry> categories)
{
    auto handler = std::make_unique<Mesher>();
    handler->categories = std::move(categories);
    return FeatureSystem::SystemHandlerPtr { handler.release() };
}
}

TEST_CASE("Navigation declarations maintain valid unique categories during setup", "[FeatureSystem][navigation]")
{
    FeatureRegistrar registrar;
    auto& navigation = registrar.navigation();
    CHECK(navigation.entryIds().empty());
    navigation.addEntry({ "", "Missing id", "", 0 });
    navigation.addEntry({ "analysis", "", "", 0 });
    CHECK(navigation.entries().empty());
    const FeatureNavigationEntry category { "analysis", "Analysis", "qrc:/analysis.svg", 1, "Simulation/Solvers" };
    navigation.addEntry(category);
    navigation.addEntry({ "analysis", "Duplicate", "", 2, "Other" });
    navigation.setLabel("Steady solver");
    navigation.setIcon("qrc:/solver.svg");
    navigation.setOrder(3);
    navigation.setEntryDefault("analysis", "iterations", "10");
    navigation.setEntryDefault("analysis", "iterations", "20");

    const auto& const_registrar = registrar;
    static_assert(std::is_same_v<decltype(registrar.navigation()), FeatureNavigation&>);
    static_assert(std::is_same_v<decltype(const_registrar.navigation()), const FeatureNavigation&>);
    const auto& declaration = const_registrar.navigation();
    static_assert(std::is_same_v<decltype(declaration.entries()), const std::vector<FeatureNavigationEntry>&>);
    CHECK(declaration.entryIds() == std::vector<std::string> { "analysis" });
    CHECK(declaration.entries() == std::vector<FeatureNavigationEntry> { category });
    CHECK(declaration.label() == "Steady solver");
    CHECK(declaration.icon() == "qrc:/solver.svg");
    CHECK(declaration.order() == 3);
    CHECK(declaration.entryDefaults().at("analysis").at("iterations") == "20");

    // 分类身份查询是派生快照，调用方修改它不能破坏后续声明。
    auto category_ids = declaration.entryIds();
    category_ids.clear();
    CHECK(declaration.entryIds() == std::vector<std::string> { "analysis" });
}

TEST_CASE("One entry declaration supports independent and shared feature icons", "[FeatureSystem][navigation]")
{
    ModelLayer model;
    core::EventBus events;
    FeatureSystem features(model, events);
    const FeatureNavigationEntry icon { "other", "Tools", "qrc:/tools.svg", 1, "Tools/Commands" };
    REQUIRE(features.registerHandler({ .name = "first" }, mesher({ icon, icon })));
    CHECK(features.getNavigationEntries() == std::vector<FeatureNavigationEntry> { icon });
    CHECK(features.getFeatureInfos().front()->navigation.entryIds() == std::vector<std::string> { "other" });
    REQUIRE(features.registerHandler({ .name = "second" }, mesher({ icon })));
    CHECK(features.getNavigationEntries() == std::vector<FeatureNavigationEntry> { icon });
    CHECK(features.getFeatureInfos().size() == 2);
    features.unregisterHandler({ .name = "first" });
    CHECK(features.getNavigationEntries() == std::vector<FeatureNavigationEntry> { icon });
    REQUIRE(features.registerHandler({ .name = "second" }, mesher({})));
    CHECK(features.getNavigationEntries().empty());
    features.unregisterHandler({ .name = "second" });
    CHECK(features.getFeatureInfos().empty());
}

TEST_CASE("Feature categories are supplied by the host and persist independently of plugins", "[FeatureSystem][navigation]")
{
    ModelLayer model;
    core::EventBus events;
    FeatureSystem features(model, events);
    CHECK(features.getNavigationEntries().empty());
    const FeatureNavigationEntry host { "analysis", "Analysis", "qrc:/analysis.svg", 1, "Simulation/Solvers" };
    features.setNavigationEntries({ host });
    CHECK(features.getNavigationEntries() == std::vector<FeatureNavigationEntry> { host });
    REQUIRE(features.registerHandler({ .name = "ordinary" }, mesher({})));
    CHECK(features.getFeatureInfos().front()->navigation.entryIds().empty());
    features.unregisterHandler({ .name = "ordinary" });
    CHECK(features.getNavigationEntries() == std::vector<FeatureNavigationEntry> { host });
    features.setNavigationEntries({});
    CHECK(features.getNavigationEntries().empty());
}

TEST_CASE("Feature categories merge deterministically and inherit only the host parent path", "[FeatureSystem][navigation]")
{
    ModelLayer model;
    core::EventBus events;
    FeatureSystem features(model, events);
    const FeatureNavigationEntry host { "analysis", "Host analysis", "qrc:/host.svg", 3, "Simulation/Solvers" };
    features.setNavigationEntries({ host });
    const auto host_categories = features.getNavigationEntries();
    const FeatureNavigationEntry first { "analysis", "First category", "qrc:/first.svg", 1 };
    const FeatureNavigationEntry second { "analysis", "Second category", "qrc:/second.svg", 2, "Extensions/Analysis" };
    auto inherited_first = first;
    inherited_first.menu_path = host.menu_path;
    REQUIRE(features.registerHandler({ .name = "z" }, mesher({ second })));
    REQUIRE(features.registerHandler({ .name = "a" }, mesher({ first })));
    const auto snapshot = features.getNavigationEntries();
    REQUIRE(snapshot.size() == 1);
    CHECK(snapshot.front() == inherited_first);
    features.unregisterHandler({ .name = "a" });
    CHECK(features.getNavigationEntries().front() == second);
    features.unregisterHandler({ .name = "z" });
    CHECK(features.getNavigationEntries() == host_categories);
    REQUIRE(features.registerHandler({ .name = "a" }, mesher({ first })));
    REQUIRE(features.registerHandler({ .name = "z" }, mesher({ second })));
    CHECK(features.getNavigationEntries() == snapshot);
    REQUIRE(features.registerHandler({ .name = "a" }, mesher({})));
    CHECK(features.getNavigationEntries().front() == second);

    // 不存在宿主同 id 时保留插件自己的父入口，不借用其他插件的路径。
    features.setNavigationEntries({});
    REQUIRE(features.registerHandler({ .name = "a" }, mesher({ first })));
    CHECK(features.getNavigationEntries().front() == first);
}

TEST_CASE("Feature categories sort by declared order and stable identity across parents", "[FeatureSystem][navigation]")
{
    ModelLayer model;
    core::EventBus events;
    FeatureSystem features(model, events);
    const FeatureNavigationEntry geometry { "geometry", "Geometry", "", 20, "Geometry/Create" };
    const FeatureNavigationEntry analysis { "analysis", "Analysis", "", 10, "Simulation/Solvers" };
    const FeatureNavigationEntry export_category { "export", "Export", "", 10, "File/Export" };
    features.setNavigationEntries({ geometry });
    REQUIRE(features.registerHandler({ .name = "exporter" }, mesher({ export_category })));
    REQUIRE(features.registerHandler({ .name = "solver" }, mesher({ analysis })));
    const std::vector<FeatureNavigationEntry> expected { analysis, export_category, geometry };
    CHECK(features.getNavigationEntries() == expected);
}

TEST_CASE("Host category changes are validated and notify only after an idle update", "[FeatureSystem][navigation]")
{
    ModelLayer model;
    core::EventBus events;
    FeatureSystem features(model, events);
    int changes = 0;
    features.setOnFeatureInfosChanged([&] { ++changes; });
    const FeatureNavigationEntry host { "analysis", "Analysis", "", 1, "Simulation/Solvers" };
    features.setNavigationEntries({ { "", "No id", "", 0 }, { "invalid", "", "", 0 }, host, { "analysis", "Duplicate", "", 0 } });
    CHECK(changes == 1);
    CHECK(features.getNavigationEntries() == std::vector<FeatureNavigationEntry> { host });
    features.setNavigationEntries({ host });
    CHECK(changes == 1);
    REQUIRE(features.registerHandler({ .name = "solver" }, mesher({ { "analysis", "Plugin analysis", "", 0 } })));
    CHECK(changes == 2);
    auto changed_host = host;
    changed_host.menu_path = "Custom/Solvers";
    features.setNavigationEntries({ changed_host });
    CHECK(changes == 3);
    CHECK(features.getNavigationEntries().front().menu_path == changed_host.menu_path);
    {
        auto occupation = model.beginWriteOperation(false);
        REQUIRE_THROWS_AS(features.setNavigationEntries({}), ModelOperationBusy);
        CHECK(changes == 3);
        CHECK(features.getNavigationEntries().front().menu_path == changed_host.menu_path);
    }
    features.unregisterHandler({ .name = "solver" });
    CHECK(features.getNavigationEntries() == std::vector<FeatureNavigationEntry> { changed_host });
}

TEST_CASE("Feature declarations discard invalid categories and reject busy registration before setup", "[FeatureSystem][navigation]")
{
    ModelLayer model;
    core::EventBus events;
    FeatureSystem features(model, events);
    REQUIRE(features.registerHandler({ .name = "mesher" }, mesher({ { "custom", "Valid", "", 1 }, { "custom", "Duplicate", "", 2 }, { "", "No id", "", 0 }, { "no-title", "", "", 0 } })));
    const auto* info = features.getFeatureInfos().front();
    CHECK(info->navigation.entryIds() == std::vector<std::string> { "custom" });
    REQUIRE(info->navigation.entries().size() == 1);
    CHECK(info->navigation.entries().front().title == "Valid");
    int calls = 0;
    auto handler = std::make_unique<Mesher>();
    handler->setup_calls = &calls;
    auto occupation = model.beginWriteOperation(false);
    REQUIRE_THROWS_AS(features.registerHandler({ .name = "busy" }, FeatureSystem::SystemHandlerPtr { handler.release() }), ModelOperationBusy);
    CHECK(calls == 0);
    CHECK(features.getFeatureInfos().size() == 1);
}

TEST_CASE("Feature replacement preserves matching parameters and initializes changed declarations", "[FeatureSystem][navigation][params]")
{
    ModelLayer model;
    core::EventBus events;
    FeatureSystem features(model, events);
    REQUIRE(features.registerHandler({ .name = "mesher" }, mesher({})));
    REQUIRE(features.setParameter("mesher", 0, core::ArgObject::create<ArgTypeEnum::Float>(9.0)));
    REQUIRE(features.registerHandler({ .name = "mesher" }, mesher({ { "custom", "New category", "", 1 } })));
    REQUIRE(features.params("mesher"));
    CHECK(*features.params("mesher")->value(0).get<ArgTypeEnum::Float>() == 9.0);
    auto changed = std::make_unique<Mesher>();
    changed->default_size = "2";
    REQUIRE(features.registerHandler({ .name = "mesher" }, FeatureSystem::SystemHandlerPtr { changed.release() }));
    CHECK(*features.params("mesher")->value(0).get<ArgTypeEnum::Float>() == 2.0);
}
