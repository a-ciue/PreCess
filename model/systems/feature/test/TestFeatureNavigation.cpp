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

using namespace systems::feature;
namespace {
class Mesher : public FeatureHandler {
public:
    std::vector<FeatureCategory> categories;
    std::string default_size = "1";
    int* setup_calls = nullptr;
    void setup(FeatureRegistrar& registrar, FeatureContext&) override
    {
        if (setup_calls)
            ++*setup_calls;
        registrar.addParameter({ ArgTypeEnum::Float, "size", default_size });
        for (const auto& category : categories)
            registrar.navigation().addCategory(category);
    }
};
FeatureSystem::SystemHandlerPtr mesher(std::vector<FeatureCategory> categories)
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
    CHECK(navigation.categories().empty());
    navigation.addCategory({ "", "Missing id", "", 0 });
    navigation.addCategory({ "other", "Reserved", "", 0 });
    navigation.addCategory({ "analysis", "", "", 0 });
    CHECK(navigation.categoryDefinitions().empty());
    const FeatureCategory category { "analysis", "Analysis", "qrc:/analysis.svg", 1, "Simulation/Solvers" };
    navigation.addCategory(category);
    navigation.addCategory({ "analysis", "Duplicate", "", 2, "Other" });
    navigation.setLabel("Steady solver");
    navigation.setGroup("Fluid");
    navigation.setIcon("qrc:/solver.svg");
    navigation.setOrder(3);
    navigation.setCategoryDefault("analysis", "iterations", "10");
    navigation.setCategoryDefault("analysis", "iterations", "20");

    const auto& const_registrar = registrar;
    static_assert(std::is_same_v<decltype(registrar.navigation()), FeatureNavigation&>);
    static_assert(std::is_same_v<decltype(const_registrar.navigation()), const FeatureNavigation&>);
    const auto& declaration = const_registrar.navigation();
    static_assert(std::is_same_v<decltype(declaration.categoryDefinitions()), const std::vector<FeatureCategory>&>);
    CHECK(declaration.categories() == std::vector<std::string> { "analysis" });
    CHECK(declaration.categoryDefinitions() == std::vector<FeatureCategory> { category });
    CHECK(declaration.label() == "Steady solver");
    CHECK(declaration.group() == "Fluid");
    CHECK(declaration.icon() == "qrc:/solver.svg");
    CHECK(declaration.order() == 3);
    CHECK(declaration.categoryDefaults().at("analysis").at("iterations") == "20");

    // 分类身份查询是派生快照，调用方修改它不能破坏后续声明。
    auto category_ids = declaration.categories();
    category_ids.clear();
    CHECK(declaration.categories() == std::vector<std::string> { "analysis" });
}

TEST_CASE("Feature categories are supplied by the host and persist independently of plugins", "[FeatureSystem][navigation]")
{
    ModelLayer model;
    core::EventBus events;
    FeatureSystem features(model, events);
    CHECK(features.getNavigationCategories().empty());
    const FeatureCategory host { "analysis", "Analysis", "qrc:/analysis.svg", 1, "Simulation/Solvers" };
    features.setNavigationCategories({ host });
    CHECK(features.getNavigationCategories() == std::vector<FeatureCategory> { host });
    REQUIRE(features.registerHandler({ .name = "ordinary" }, mesher({})));
    CHECK(features.getFeatureInfos().front()->navigation.categories().empty());
    features.unregisterHandler({ .name = "ordinary" });
    CHECK(features.getNavigationCategories() == std::vector<FeatureCategory> { host });
    features.setNavigationCategories({});
    CHECK(features.getNavigationCategories().empty());
}

TEST_CASE("Feature categories merge deterministically and inherit only the host parent path", "[FeatureSystem][navigation]")
{
    ModelLayer model;
    core::EventBus events;
    FeatureSystem features(model, events);
    const FeatureCategory host { "analysis", "Host analysis", "qrc:/host.svg", 3, "Simulation/Solvers" };
    features.setNavigationCategories({ host });
    const auto host_categories = features.getNavigationCategories();
    const FeatureCategory first { "analysis", "First category", "qrc:/first.svg", 1 };
    const FeatureCategory second { "analysis", "Second category", "qrc:/second.svg", 2, "Extensions/Analysis" };
    auto inherited_first = first;
    inherited_first.menu_path = host.menu_path;
    REQUIRE(features.registerHandler({ .name = "z" }, mesher({ second })));
    REQUIRE(features.registerHandler({ .name = "a" }, mesher({ first })));
    const auto snapshot = features.getNavigationCategories();
    REQUIRE(snapshot.size() == 1);
    CHECK(snapshot.front() == inherited_first);
    features.unregisterHandler({ .name = "a" });
    CHECK(features.getNavigationCategories().front() == second);
    features.unregisterHandler({ .name = "z" });
    CHECK(features.getNavigationCategories() == host_categories);
    REQUIRE(features.registerHandler({ .name = "a" }, mesher({ first })));
    REQUIRE(features.registerHandler({ .name = "z" }, mesher({ second })));
    CHECK(features.getNavigationCategories() == snapshot);
    REQUIRE(features.registerHandler({ .name = "a" }, mesher({})));
    CHECK(features.getNavigationCategories().front() == second);

    // 不存在宿主同 id 时保留插件自己的父入口，不借用其他插件的路径。
    features.setNavigationCategories({});
    REQUIRE(features.registerHandler({ .name = "a" }, mesher({ first })));
    CHECK(features.getNavigationCategories().front() == first);
}

TEST_CASE("Feature categories sort by declared order and stable identity across parents", "[FeatureSystem][navigation]")
{
    ModelLayer model;
    core::EventBus events;
    FeatureSystem features(model, events);
    const FeatureCategory geometry { "geometry", "Geometry", "", 20, "Geometry/Create" };
    const FeatureCategory analysis { "analysis", "Analysis", "", 10, "Simulation/Solvers" };
    const FeatureCategory export_category { "export", "Export", "", 10, "File/Export" };
    features.setNavigationCategories({ geometry });
    REQUIRE(features.registerHandler({ .name = "exporter" }, mesher({ export_category })));
    REQUIRE(features.registerHandler({ .name = "solver" }, mesher({ analysis })));
    const std::vector<FeatureCategory> expected { analysis, export_category, geometry };
    CHECK(features.getNavigationCategories() == expected);
}

TEST_CASE("Host category changes are validated and notify only after an idle update", "[FeatureSystem][navigation]")
{
    ModelLayer model;
    core::EventBus events;
    FeatureSystem features(model, events);
    int changes = 0;
    features.setOnFeatureInfosChanged([&] { ++changes; });
    const FeatureCategory host { "analysis", "Analysis", "", 1, "Simulation/Solvers" };
    features.setNavigationCategories({ { "", "No id", "", 0 }, { "other", "Reserved", "", 0 }, { "invalid", "", "", 0 }, host, { "analysis", "Duplicate", "", 0 } });
    CHECK(changes == 1);
    CHECK(features.getNavigationCategories() == std::vector<FeatureCategory> { host });
    features.setNavigationCategories({ host });
    CHECK(changes == 1);
    REQUIRE(features.registerHandler({ .name = "solver" }, mesher({ { "analysis", "Plugin analysis", "", 0 } })));
    CHECK(changes == 2);
    auto changed_host = host;
    changed_host.menu_path = "Custom/Solvers";
    features.setNavigationCategories({ changed_host });
    CHECK(changes == 3);
    CHECK(features.getNavigationCategories().front().menu_path == changed_host.menu_path);
    {
        auto occupation = model.beginWriteOperation(false);
        REQUIRE_THROWS_AS(features.setNavigationCategories({}), ModelOperationBusy);
        CHECK(changes == 3);
        CHECK(features.getNavigationCategories().front().menu_path == changed_host.menu_path);
    }
    features.unregisterHandler({ .name = "solver" });
    CHECK(features.getNavigationCategories() == std::vector<FeatureCategory> { changed_host });
}

TEST_CASE("Feature declarations discard invalid categories and reject busy registration before setup", "[FeatureSystem][navigation]")
{
    ModelLayer model;
    core::EventBus events;
    FeatureSystem features(model, events);
    REQUIRE(features.registerHandler({ .name = "mesher" }, mesher({ { "custom", "Valid", "", 1 }, { "custom", "Duplicate", "", 2 }, { "other", "Reserved", "", 0 }, { "", "No id", "", 0 }, { "no-title", "", "", 0 } })));
    const auto* info = features.getFeatureInfos().front();
    CHECK(info->navigation.categories() == std::vector<std::string> { "custom" });
    REQUIRE(info->navigation.categoryDefinitions().size() == 1);
    CHECK(info->navigation.categoryDefinitions().front().title == "Valid");
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
