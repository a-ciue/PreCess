#include "ComponentData.h"
#include "CreateLineByCoordinatesHandler.h"
#include "EventBus.h"
#include "FeatureSystem.h"
#include "GeometryData.h"
#include "MeshData.h"
#include "ModelLayer.h"

#include <catch2/catch_test_macros.hpp>

#include <any>

using namespace systems;
using namespace systems::feature;

namespace {
HandlerMetaData lineMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "CreateLineByCoordinates";
    meta_data.display_name = "创建直线边（坐标）";
    return meta_data;
}
}

TEST_CASE("CreateLineByCoordinates execute creates line component by write target", "[CreateLineByCoordinatesPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);

    FeatureSystem::SystemHandlerPtr handler { new CreateLineByCoordinatesHandler };
    REQUIRE(feature_system.registerHandler(lineMetaData(), std::move(handler)));

    // 默认写入目标 0（添加到当前 Component）且无活动组件：返回界面提示语，不产生组件
    const std::any hint = feature_system.invoke("CreateLineByCoordinates");
    REQUIRE(std::any_cast<std::string>(hint) == "请选择当前 Component，或修改写入目标。");

    // 写入目标 2：新建临时模型承载，返回新组件 id
    REQUIRE(feature_system.setParameter("CreateLineByCoordinates", 6, core::ArgObject::create<ArgTypeEnum::Combo>(2)));
    const Index component_id = std::any_cast<Index>(feature_system.invoke("CreateLineByCoordinates"));
    const auto* component = model_layer.findComponent(component_id);
    REQUIRE(component != nullptr);
    REQUIRE(component->name == "Line_1");
    REQUIRE(component->geometry != nullptr);
    REQUIRE(component->geometry->rootShape != nullptr);
}
