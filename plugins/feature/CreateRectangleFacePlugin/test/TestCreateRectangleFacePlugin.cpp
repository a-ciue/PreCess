#include "ComponentData.h"
#include "CreateRectangleFaceHandler.h"
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
HandlerMetaData rectangleMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "CreateRectangleFace";
    meta_data.display_name = "创建矩形面";
    return meta_data;
}
}

TEST_CASE("CreateRectangleFace execute creates face component by write target", "[CreateRectangleFacePlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);

    FeatureSystem::SystemHandlerPtr handler { new CreateRectangleFaceHandler };
    REQUIRE(feature_system.registerHandler(rectangleMetaData(), std::move(handler)));

    // 写入目标 2：新建临时模型承载，返回新组件 id（默认参数 10x10 矩形）
    REQUIRE(feature_system.setParameter("CreateRectangleFace", 6, core::ArgObject::create<ArgTypeEnum::Combo>(2)));
    const Index component_id = std::any_cast<Index>(feature_system.invoke("CreateRectangleFace"));
    const auto* component = model_layer.findComponent(component_id);
    REQUIRE(component != nullptr);
    REQUIRE(component->name == "RectangleFace_1");
    REQUIRE(component->geometry != nullptr);
    REQUIRE(component->geometry->rootShape != nullptr);

    // 索引建成后应含 1 个面、4 条边、4 个顶点（共享拓扑）
    component->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    const auto& index = component->geometry->index;
    REQUIRE(index.face_local_to_global.size() == 1 + 1); // 局部 id 从 1 起，0 号为保留槽
    REQUIRE(index.edge_local_to_global.size() == 4 + 1);
    REQUIRE(index.vertex_local_to_global.size() == 4 + 1);
}
