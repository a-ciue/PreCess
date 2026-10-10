#include "CollapseEdgeHandler.h"
#include "EventBus.h"
#include "FeatureSystem.h"
#include "ModelLayer.h"
#include <catch2/catch_test_macros.hpp>
#include <any>
#include <string>
#include <utility>
using namespace systems;
using namespace systems::feature;
TEST_CASE("CollapseEdge feature registers its baseline parameters", "[CollapseEdgePlugin]")
{
    core::EventBus bus;
    ModelLayer model;
    FeatureSystem system(model, bus);
    HandlerMetaData meta;
    meta.name = "CollapseEdge";
    FeatureSystem::SystemHandlerPtr handler { new CollapseEdgeHandler };
    REQUIRE(system.registerHandler(meta, std::move(handler)));
    const auto navigation_entries = system.getNavigationEntries();
    REQUIRE(navigation_entries.size() == 1);
    CHECK(navigation_entries.front().id == "CollapseEdge");
    CHECK(navigation_entries.front().title == "压缩边");
    CHECK(navigation_entries.front().menu_path == "几何/拓扑");
    REQUIRE(std::any_cast<std::string>(system.invoke("CollapseEdge")) == "请选择一条需要压缩的几何边。");
}
