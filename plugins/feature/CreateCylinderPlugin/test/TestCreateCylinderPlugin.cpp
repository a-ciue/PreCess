#include "ComponentData.h"
#include "CreateCylinderHandler.h"
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
HandlerMetaData cylinderMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "CreateCylinder";
    meta_data.display_name = "创建圆柱体";
    return meta_data;
}
}

TEST_CASE("CreateCylinder execute creates cylinder component by write target", "[CreateCylinderPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);

    FeatureSystem::SystemHandlerPtr handler { new CreateCylinderHandler };
    REQUIRE(feature_system.registerHandler(cylinderMetaData(), std::move(handler)));

    // 写入目标 2：新建临时模型承载（默认参数为完整圆柱）
    REQUIRE(feature_system.setParameter("CreateCylinder", 9, core::ArgObject::create<ArgTypeEnum::Combo>(2)));
    const Index component_id = std::any_cast<Index>(feature_system.invoke("CreateCylinder"));
    const auto* component = model_layer.findComponent(component_id);
    REQUIRE(component != nullptr);
    REQUIRE(component->name == "Cylinder_1");
    REQUIRE(component->geometry != nullptr);
    REQUIRE(component->geometry->rootShape != nullptr);

    // 完整圆柱：2 个面（侧面 + 底面合环）与 2 个顶点
    component->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    REQUIRE(component->geometry->index.face_local_to_global.size() == 3 + 1); // 侧面 + 两底面；局部 id 从 1 起
    REQUIRE(component->geometry->index.vertex_local_to_global.size() == 2 + 1);
}
