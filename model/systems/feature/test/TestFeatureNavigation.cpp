/** @file TestFeatureNavigation.cpp
 * @brief 网格生成 Feature 的分类聚合及持久参数回归测试。
 */
#include "ArgObject.h"
#include "FeatureHandler.h"
#include "FeatureRegistrar.h"
#include "FeatureSystem.h"
#include "ModelLayer.h"
#include <catch2/catch_test_macros.hpp>

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
            registrar.addCategory(category);
    }
};
FeatureSystem::SystemHandlerPtr mesher(std::vector<FeatureCategory> categories)
{
    auto handler = std::make_unique<Mesher>();
    handler->categories = std::move(categories);
    return FeatureSystem::SystemHandlerPtr { handler.release() };
}
}

TEST_CASE("Feature mesh categories merge deterministically and retain the four base entries", "[FeatureSystem][navigation]")
{
    ModelLayer model;
    core::EventBus events;
    FeatureSystem features(model, events);
    const auto builtins = features.getNavigationCategories();
    REQUIRE(builtins.size() == 4);
    CHECK(builtins[3].id == "hexahedron");
    const FeatureCategory first { "custom", "First category", "qrc:/first.svg", 1 };
    const FeatureCategory second { "custom", "Second category", "qrc:/second.svg", 2 };
    REQUIRE(features.registerHandler({ .name = "z" }, mesher({ second })));
    REQUIRE(features.registerHandler({ .name = "a" }, mesher({ first })));
    const auto snapshot = features.getNavigationCategories();
    REQUIRE(snapshot.size() == 5);
    CHECK(snapshot.front() == first);
    features.unregisterHandler({ .name = "a" });
    CHECK(features.getNavigationCategories().front() == second);
    features.unregisterHandler({ .name = "z" });
    CHECK(features.getNavigationCategories() == builtins);
    REQUIRE(features.registerHandler({ .name = "a" }, mesher({ first })));
    REQUIRE(features.registerHandler({ .name = "z" }, mesher({ second })));
    CHECK(features.getNavigationCategories() == snapshot);
    REQUIRE(features.registerHandler({ .name = "a" }, mesher({})));
    CHECK(features.getNavigationCategories().front() == second);
}

TEST_CASE("Feature mesh declarations discard invalid categories and reject busy registration before setup", "[FeatureSystem][navigation]")
{
    ModelLayer model;
    core::EventBus events;
    FeatureSystem features(model, events);
    REQUIRE(features.registerHandler({ .name = "mesher" }, mesher({ { "custom", "Valid", "", 1 }, { "custom", "Duplicate", "", 2 }, { "other", "Reserved", "", 0 }, { "", "No id", "", 0 }, { "no-title", "", "", 0 } })));
    const auto* info = features.getFeatureInfos().front();
    CHECK(info->navigation.categories == std::vector<std::string> { "custom" });
    REQUIRE(info->navigation.category_definitions.size() == 1);
    CHECK(info->navigation.category_definitions.front().title == "Valid");
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
