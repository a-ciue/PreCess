/** @file TestFeatureNavigation.cpp
 * @brief 导航父节点注册、入口聚合及参数保持的回归测试。
 */
#include "ArgObject.h"
#include "FeatureHandler.h"
#include "FeatureRegistrar.h"
#include "FeatureSystem.h"
#include "ModelLayer.h"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

using namespace systems::feature;
namespace {
class Mesher : public FeatureHandler {
public:
    std::vector<FeatureNavigationMenu> menus;
    std::vector<FeatureNavigationGroup> groups;
    std::vector<FeatureNavigationEntry> categories;
    double order { 0 };
    std::string default_size = "1";
    int* setup_calls = nullptr;
    bool fail_setup = false;
    void setup(FeatureRegistrar& registrar, FeatureContext&) override
    {
        if (setup_calls)
            ++*setup_calls;
        registrar.addParameter({ ArgTypeEnum::Float, "size", default_size });
        for (const auto& menu : menus)
            registrar.addNavigationMenu(menu);
        for (const auto& group : groups)
            registrar.addNavigationGroup(group);
        registrar.navigation().setOrder(order);
        for (const auto& category : categories)
            registrar.navigation().addEntry(category);
        if (fail_setup)
            throw std::runtime_error("setup failed after collecting navigation");
    }
};
FeatureSystem::SystemHandlerPtr mesher(std::vector<FeatureNavigationEntry> categories)
{
    auto handler = std::make_unique<Mesher>();
    handler->menus = { { "tools", "Tools", 60 }, { "simulation", "Simulation", 70 }, { "extensions", "Extensions", 80 } };
    handler->groups = { { "tools", "commands", "Commands" }, { "simulation", "solvers", "Solvers" } };
    handler->categories = std::move(categories);
    return FeatureSystem::SystemHandlerPtr { handler.release() };
}
void registerTestParents(FeatureSystem& features)
{
    features.tryRegisterNavigationMenu({ "simulation", "Simulation", 70 });
    features.tryRegisterNavigationGroup({ "simulation", "solvers", "Solvers" });
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
    const FeatureNavigationEntry category { "analysis", "Analysis", "qrc:/analysis.svg", 1.5, "simulation", "solvers" };
    navigation.addEntry(category);
    navigation.addEntry({ "analysis", "Duplicate", "", 2, "other" });
    navigation.setLabel("Steady solver");
    navigation.setIcon("qrc:/solver.svg");
    navigation.setOrder(3.25);
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
    CHECK(declaration.order() == 3.25);
    for (const double invalid : { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity() }) {
        CHECK_THROWS_AS(navigation.setOrder(invalid), std::invalid_argument);
        CHECK(declaration.order() == 3.25);
    }
    CHECK(declaration.entryDefaults().at("analysis").at("iterations") == "20");
    auto category_ids = declaration.entryIds();
    category_ids.clear();
    CHECK(declaration.entryIds() == std::vector<std::string> { "analysis" });
}

TEST_CASE("One entry declaration supports independent and shared feature icons", "[FeatureSystem][navigation]")
{
    ModelLayer model;
    core::EventBus events;
    FeatureSystem features(model, events);
    const FeatureNavigationEntry icon { "other", "Tools", "qrc:/tools.svg", 1, "tools", "commands" };
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

TEST_CASE("Host entries persist independently of plugins", "[FeatureSystem][navigation]")
{
    ModelLayer model;
    core::EventBus events;
    FeatureSystem features(model, events);
    registerTestParents(features);
    const FeatureNavigationEntry host { "analysis", "Analysis", "qrc:/analysis.svg", 1.5, "simulation", "solvers" };
    features.setNavigationEntries({ host });
    CHECK(features.getNavigationEntries() == std::vector<FeatureNavigationEntry> { host });
    REQUIRE(features.registerHandler({ .name = "ordinary" }, mesher({})));
    CHECK(features.getFeatureInfos().front()->navigation.entryIds().empty());
    features.unregisterHandler({ .name = "ordinary" });
    CHECK(features.getNavigationEntries() == std::vector<FeatureNavigationEntry> { host });
    features.setNavigationEntries({});
    CHECK(features.getNavigationEntries().empty());
    CHECK_FALSE(features.getNavigationMenus().empty());
}

TEST_CASE("Shared entry providers remain deterministic with explicit parent references", "[FeatureSystem][navigation]")
{
    ModelLayer model;
    core::EventBus events;
    FeatureSystem features(model, events);
    registerTestParents(features);
    const FeatureNavigationEntry host { "analysis", "Host analysis", "qrc:/host.svg", 3, "simulation", "solvers" };
    const FeatureNavigationEntry first { "analysis", "First category", "qrc:/first.svg", 1, "simulation", "solvers" };
    const FeatureNavigationEntry second { "analysis", "Second category", "qrc:/second.svg", 2, "extensions" };
    features.setNavigationEntries({ host });
    REQUIRE(features.registerHandler({ .name = "z" }, mesher({ second })));
    REQUIRE(features.registerHandler({ .name = "a" }, mesher({ first })));
    CHECK(features.getNavigationEntries() == std::vector<FeatureNavigationEntry> { first });
    features.unregisterHandler({ .name = "a" });
    CHECK(features.getNavigationEntries() == std::vector<FeatureNavigationEntry> { second });
    features.unregisterHandler({ .name = "z" });
    CHECK(features.getNavigationEntries() == std::vector<FeatureNavigationEntry> { host });
    REQUIRE(features.registerHandler({ .name = "a" }, mesher({ first })));
    REQUIRE(features.registerHandler({ .name = "z" }, mesher({ second })));
    CHECK(features.getNavigationEntries() == std::vector<FeatureNavigationEntry> { first });
    features.setNavigationEntries({});
    CHECK(features.getNavigationEntries() == std::vector<FeatureNavigationEntry> { first });
}

TEST_CASE("Entries sort by fractional order then stable identity across parents", "[FeatureSystem][navigation]")
{
    ModelLayer model;
    core::EventBus events;
    FeatureSystem features(model, events);
    const FeatureNavigationEntry first { "z", "First", "", -10.75, "tools" };
    const FeatureNavigationEntry second { "a", "Second", "", -10.25, "simulation", "solvers" };
    const FeatureNavigationEntry third { "b", "Third", "", -10.25, "extensions" };
    REQUIRE(features.registerHandler({ .name = "mesher" }, mesher({ third, second, first })));
    CHECK(features.getNavigationEntries() == std::vector<FeatureNavigationEntry> { first, second, third });
}

TEST_CASE("Host entry changes validate parents atomically and notify only on idle updates", "[FeatureSystem][navigation]")
{
    ModelLayer model;
    core::EventBus events;
    FeatureSystem features(model, events);
    registerTestParents(features);
    int changes = 0;
    features.setOnFeatureInfosChanged([&] { ++changes; });
    const FeatureNavigationEntry host { "analysis", "Analysis", "", 1, "simulation", "solvers" };
    features.setNavigationEntries({ { "", "No id", "", 0 }, { "invalid", "", "", 0 }, host, { "analysis", "Duplicate", "", 0 } });
    CHECK(changes == 1);
    features.setNavigationEntries({ host });
    CHECK(changes == 1);
    auto changed_host = host;
    changed_host.group_id.clear();
    features.setNavigationEntries({ changed_host });
    CHECK(changes == 2);
    CHECK(features.getNavigationEntries() == std::vector<FeatureNavigationEntry> { changed_host });
    auto invalid = host;
    invalid.menu_id = "Simulation/Solvers";
    CHECK_THROWS_AS(features.setNavigationEntries({ invalid }), std::invalid_argument);
    CHECK(changes == 2);
    CHECK(features.getNavigationEntries() == std::vector<FeatureNavigationEntry> { changed_host });
    {
        auto occupation = model.beginWriteOperation(false);
        REQUIRE_THROWS_AS(features.setNavigationEntries({}), ModelOperationBusy);
        CHECK(changes == 2);
        CHECK(features.getNavigationEntries() == std::vector<FeatureNavigationEntry> { changed_host });
    }
}

TEST_CASE("Feature declarations filter invalid entries and reject busy registration before setup", "[FeatureSystem][navigation]")
{
    ModelLayer model;
    core::EventBus events;
    FeatureSystem features(model, events);
    REQUIRE(features.registerHandler({ .name = "mesher" }, mesher({ { "custom", "Valid", "", 1, "tools" }, { "custom", "Duplicate", "", 2 }, { "", "No id", "", 0 }, { "no-title", "", "", 0 } })));
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
    REQUIRE(features.registerHandler({ .name = "mesher" }, mesher({ { "custom", "New category", "", 1, "tools" } })));
    REQUIRE(features.params("mesher"));
    CHECK(*features.params("mesher")->value(0).get<ArgTypeEnum::Float>() == 9.0);
    auto changed = std::make_unique<Mesher>();
    changed->default_size = "2";
    REQUIRE(features.registerHandler({ .name = "mesher" }, FeatureSystem::SystemHandlerPtr { changed.release() }));
    CHECK(*features.params("mesher")->value(0).get<ArgTypeEnum::Float>() == 2.0);
}

TEST_CASE("Host order overrides validate atomically and persist through plugin changes", "[FeatureSystem][navigation]")
{
    ModelLayer model;
    core::EventBus events;
    FeatureSystem features(model, events);
    int changes = 0;
    features.setOnFeatureInfosChanged([&] { ++changes; });
    const std::vector<FeatureNavigationOrder> expected { { "entry:quad", "shared", 10.25 }, { "group:mesh/default", "category:quad", 0 } };
    features.setNavigationOrder({ expected[0], expected[1], { "group:mesh/default", "category:quad", 100 }, { "", "", -1 } });
    CHECK(features.getNavigationOrder() == expected);
    const auto before = changes;
    features.setNavigationOrder(expected);
    CHECK(changes == before);
    CHECK_THROWS_AS(features.setNavigationOrder({ { "", "mesh", 0 } }), std::invalid_argument);
    CHECK_THROWS_AS(features.setNavigationOrder({ { "menu:mesh", "surface", 0 } }), std::invalid_argument);
    for (const double invalid : { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity() }) {
        CHECK_THROWS_AS(features.setNavigationOrder({ { "entry:quad", "shared", 1 }, { "entry:quad", "other", invalid } }), std::invalid_argument);
        CHECK(features.getNavigationOrder() == expected);
    }
    CHECK(changes == before);
    {
        auto occupation = model.beginWriteOperation(false);
        CHECK_THROWS_AS(features.setNavigationOrder({}), ModelOperationBusy);
    }
    REQUIRE(features.registerHandler({ .name = "mesher" }, mesher({})));
    CHECK(features.getNavigationOrder() == expected);
    features.unregisterHandler({ .name = "mesher" });
    CHECK(features.getNavigationOrder() == expected);
    features.setNavigationOrder({});
    CHECK(features.getNavigationOrder().empty());
}

TEST_CASE("Navigation menus and groups keep first successful definitions and scoped identities", "[FeatureSystem][navigation]")
{
    ModelLayer model;
    core::EventBus events;
    FeatureSystem features(model, events);
    int changes = 0;
    features.setOnFeatureInfosChanged([&] { ++changes; });
    CHECK(FeatureNavigationMenu {}.order == 1000);
    CHECK(FeatureNavigationGroup {}.order == 0);
    REQUIRE(features.tryRegisterNavigationMenu({ "geometry", "几何", 50.5 }));
    CHECK_FALSE(features.tryRegisterNavigationMenu({ "geometry", "Replacement", -1 }));
    REQUIRE(features.tryRegisterNavigationMenu({ "analysis", "Analysis", 50.5 }));
    REQUIRE(features.tryRegisterNavigationGroup({ "geometry", "repair", "修复", 30 }));
    CHECK_FALSE(features.tryRegisterNavigationGroup({ "geometry", "repair", "Replacement", -1 }));
    REQUIRE(features.tryRegisterNavigationGroup({ "analysis", "repair", "Analysis repair", 30 }));
    REQUIRE(features.tryRegisterNavigationGroup({ "geometry", "", "", 10 }));
    REQUIRE(features.tryRegisterNavigationGroup({ "geometry", "topology", "拓扑", 20 }));
    CHECK(changes == 6);
    const auto menus = features.getNavigationMenus();
    const auto groups = features.getNavigationGroups();
    CHECK(menus.size() == 2);
    CHECK(groups.size() == 4);
    for (const auto& expected : std::vector<FeatureNavigationMenu> { { "analysis", "Analysis", 50.5 }, { "geometry", "几何", 50.5 } })
        CHECK(std::find(menus.begin(), menus.end(), expected) != menus.end());
    for (const auto& expected : std::vector<FeatureNavigationGroup> { { "analysis", "repair", "Analysis repair", 30 }, { "geometry", "", "", 10 }, { "geometry", "topology", "拓扑", 20 }, { "geometry", "repair", "修复", 30 } })
        CHECK(std::find(groups.begin(), groups.end(), expected) != groups.end());
    auto plugin = std::make_unique<Mesher>();
    plugin->menus = { { "geometry", "Plugin geometry", -100 } };
    plugin->groups = { { "geometry", "repair", "Plugin repair", -100 } };
    plugin->categories = { { "fix", "Fix", "", 0, "geometry", "repair" } };
    REQUIRE(features.registerHandler({ .name = "a" }, FeatureSystem::SystemHandlerPtr { plugin.release() }));
    CHECK(changes == 7);
    CHECK(features.getNavigationMenus() == menus);
    CHECK(features.getNavigationGroups() == groups);
    features.unregisterHandler({ .name = "a" });
    CHECK(features.getNavigationMenus() == menus);
    CHECK(features.getNavigationGroups() == groups);
    CHECK(features.getNavigationEntries().empty());
}

TEST_CASE("Plugin parent definitions follow successful registration order rather than provider names", "[FeatureSystem][navigation]")
{
    ModelLayer model;
    core::EventBus events;
    FeatureSystem features(model, events);
    const auto install = [&](const std::string& name, const std::string& title) {
        auto plugin = std::make_unique<Mesher>();
        plugin->menus = { { "custom", title, 70 }, { "custom", "Ignored same-setup duplicate", -10 } };
        plugin->groups = { { "custom", "commands", title, 20 }, { "custom", "commands", "Ignored", -10 } };
        plugin->categories = { { "shared", title, "", 0, "custom", "commands" } };
        REQUIRE(features.registerHandler({ .name = name }, FeatureSystem::SystemHandlerPtr { plugin.release() }));
    };
    install("z", "First");
    install("a", "Second");
    CHECK(features.getNavigationMenus().front().title == "First");
    CHECK(features.getNavigationGroups().front().title == "First");
    CHECK(features.getNavigationEntries().front().title == "Second");
    features.unregisterHandler({ .name = "z" });
    features.unregisterHandler({ .name = "a" });
    install("b", "Third");
    CHECK(features.getNavigationMenus().front().title == "First");
    CHECK(features.getNavigationGroups().front().title == "First");
}

TEST_CASE("Failed feature registration cannot claim parent definitions or leave feature information", "[FeatureSystem][navigation]")
{
    ModelLayer model;
    core::EventBus events;
    FeatureSystem features(model, events);
    int changes = 0;
    features.setOnFeatureInfosChanged([&] { ++changes; });
    auto plugin = std::make_unique<Mesher>();
    plugin->menus = { { "custom", "Failed", 1 } };
    plugin->groups = { { "custom", "commands", "Failed", 1 } };
    plugin->categories = { { "action", "Action", "", 0, "custom", "commands" } };
    SECTION("setup throws after collecting declarations")
    {
        plugin->fail_setup = true;
    }
    SECTION("group refers to an unknown menu")
    {
        plugin->groups.push_back({ "unknown", "commands", "Unknown" });
    }
    SECTION("a later menu uses a path separator in its identity")
    {
        plugin->menus.push_back({ "custom/nested", "Nested" });
    }
    SECTION("a later group uses a path separator in its identity")
    {
        plugin->groups.push_back({ "custom", "commands/nested", "Nested" });
    }
    SECTION("entry order is not finite")
    {
        plugin->categories.front().order = std::numeric_limits<double>::quiet_NaN();
    }
    SECTION("subfeature order is not finite")
    {
        plugin->order = std::numeric_limits<double>::infinity();
    }
    SECTION("entry has no menu id")
    {
        plugin->categories.front().menu_id.clear();
    }
    SECTION("entry refers to another menu group")
    {
        plugin->menus.push_back({ "other", "Other" });
        plugin->categories.front().menu_id = "other";
    }
    SECTION("legacy path accidentally occupies the menu id field")
    {
        plugin->categories.front().menu_id = "Custom/Commands";
    }
    REQUIRE_THROWS(features.registerHandler({ .name = "failed" }, FeatureSystem::SystemHandlerPtr { plugin.release() }));
    CHECK(features.getFeatureInfos().empty());
    CHECK(features.params("failed") == nullptr);
    CHECK(features.getNavigationEntries().empty());
    CHECK(features.getNavigationMenus().empty());
    CHECK(features.getNavigationGroups().empty());
    CHECK(changes == 0);
    REQUIRE(features.tryRegisterNavigationMenu({ "custom", "Successful", 100 }));
    CHECK(features.getNavigationMenus().front().title == "Successful");
}

TEST_CASE("Direct parent registration validates definitions and respects model occupation", "[FeatureSystem][navigation]")
{
    ModelLayer model;
    core::EventBus events;
    FeatureSystem features(model, events);
    CHECK_THROWS_AS(features.tryRegisterNavigationMenu({ "", "No id" }), std::invalid_argument);
    CHECK_THROWS_AS(features.tryRegisterNavigationMenu({ "empty", "" }), std::invalid_argument);
    CHECK_THROWS_AS(features.tryRegisterNavigationMenu({ "geometry/repair", "Invalid identity" }), std::invalid_argument);
    CHECK_THROWS_AS(features.tryRegisterNavigationMenu({ "nan", "NaN", std::numeric_limits<double>::quiet_NaN() }), std::invalid_argument);
    CHECK_THROWS_AS(features.tryRegisterNavigationGroup({ "missing", "group", "Group" }), std::invalid_argument);
    REQUIRE(features.tryRegisterNavigationMenu({ "geometry", "Geometry/Build", 50 }));
    CHECK(features.getNavigationMenus().front().title == "Geometry/Build");
    CHECK_THROWS_AS(features.tryRegisterNavigationGroup({ "geometry", "", "Named anonymous group" }), std::invalid_argument);
    CHECK_THROWS_AS(features.tryRegisterNavigationGroup({ "geometry", "group", "" }), std::invalid_argument);
    CHECK_THROWS_AS(features.tryRegisterNavigationGroup({ "geometry", "default", "Reserved" }), std::invalid_argument);
    CHECK_THROWS_AS(features.tryRegisterNavigationGroup({ "geometry", "repair/heal", "Invalid identity" }), std::invalid_argument);
    // 分隔符只限制身份，显示标题不参与排序键的拼接。
    REQUIRE(features.tryRegisterNavigationGroup({ "geometry", "repair", "Repair/Heal" }));
    CHECK(features.getNavigationGroups().front().title == "Repair/Heal");
    auto occupation = model.beginWriteOperation(false);
    CHECK_THROWS_AS(features.tryRegisterNavigationMenu({ "new", "New" }), ModelOperationBusy);
    CHECK_THROWS_AS(features.tryRegisterNavigationMenu({ "geometry", "Duplicate" }), ModelOperationBusy);
    CHECK_THROWS_AS(features.tryRegisterNavigationGroup({ "geometry", "new", "New" }), ModelOperationBusy);
    CHECK(features.getNavigationMenus().size() == 1);
    CHECK(features.getNavigationGroups().size() == 1);
}
