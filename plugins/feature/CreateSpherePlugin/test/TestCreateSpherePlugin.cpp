#include "ComponentData.h"
#include "CreateSphereHandler.h"
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
HandlerMetaData sphereMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "CreateSphere";
    meta_data.display_name = "创建球体/部分球体";
    return meta_data;
}
}

TEST_CASE("CreateSphere execute creates sphere component by write target", "[CreateSpherePlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);

    FeatureSystem::SystemHandlerPtr handler { new CreateSphereHandler };
    REQUIRE(feature_system.registerHandler(sphereMetaData(), std::move(handler)));

    // 写入目标 2：新建临时模型承载（默认参数为完整球体）
    REQUIRE(feature_system.setParameter("CreateSphere", 10, core::ArgObject::create<ArgTypeEnum::Combo>(2)));
    const Index component_id = std::any_cast<Index>(feature_system.invoke("CreateSphere"));
    const auto* component = model_layer.findComponent(component_id);
    REQUIRE(component != nullptr);
    REQUIRE(component->name == "Sphere_1");
    REQUIRE(component->geometry != nullptr);
    REQUIRE(component->geometry->rootShape != nullptr);

    // 完整球体：1 个面、无边、无独立顶点
    component->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    REQUIRE(component->geometry->index.face_local_to_global.size() == 1 + 1); // 局部 id 从 1 起，0 号为保留槽
    REQUIRE(component->geometry->index.edge_local_to_global.size() == 3 + 1); // 球面含缝合边与两极圈
    REQUIRE(component->geometry->index.vertex_local_to_global.size() == 2 + 1); // 两个极点
}
