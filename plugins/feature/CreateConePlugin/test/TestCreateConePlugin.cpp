#include "ComponentData.h"
#include "CreateConeHandler.h"
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
HandlerMetaData coneMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "CreateCone";
    meta_data.display_name = "创建圆锥/圆台";
    return meta_data;
}
}

TEST_CASE("CreateCone execute creates cone component by write target", "[CreateConePlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);

    FeatureSystem::SystemHandlerPtr handler { new CreateConeHandler };
    REQUIRE(feature_system.registerHandler(coneMetaData(), std::move(handler)));

    // 写入目标 2：新建临时模型承载（默认参数为圆台）
    REQUIRE(feature_system.setParameter("CreateCone", 10, core::ArgObject::create<ArgTypeEnum::Combo>(2)));
    const Index component_id = std::any_cast<Index>(feature_system.invoke("CreateCone"));
    const auto* component = model_layer.findComponent(component_id);
    REQUIRE(component != nullptr);
    REQUIRE(component->name == "Cone_1");
    REQUIRE(component->geometry != nullptr);
    REQUIRE(component->geometry->rootShape != nullptr);

    // 圆台：3 个面（侧面 + 底面 + 顶面）
    component->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    REQUIRE(component->geometry->index.face_local_to_global.size() == 3 + 1); // 局部 id 从 1 起，0 号为保留槽
}
