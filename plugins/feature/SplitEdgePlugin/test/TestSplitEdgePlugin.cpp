#include "EventBus.h"
#include "FeatureSystem.h"
#include "ModelLayer.h"
#include "SplitEdgeHandler.h"
#include <catch2/catch_test_macros.hpp>
#include <any>
#include <string>
#include <utility>
using namespace systems;
using namespace systems::feature;
TEST_CASE("SplitEdge feature registers its baseline parameters", "[SplitEdgePlugin]")
{
    core::EventBus bus;
    ModelLayer model;
    FeatureSystem system(model, bus);
    HandlerMetaData meta;
    meta.name = "SplitEdge";
    FeatureSystem::SystemHandlerPtr handler { new SplitEdgeHandler };
    REQUIRE(system.registerHandler(meta, std::move(handler)));
    REQUIRE(std::any_cast<std::string>(system.invoke("SplitEdge")) == "请选择一条需要分割的几何边。");
}
