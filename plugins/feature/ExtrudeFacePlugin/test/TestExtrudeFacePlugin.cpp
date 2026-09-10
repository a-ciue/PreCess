#include "ComponentData.h"
#include "ExtrudeFaceHandler.h"
#include "EventBus.h"
#include "GeometryData.h"
#include "FeatureSystem.h"
#include "GeometryBuilder.h"
#include "ModelOperator.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "Selection.h"

#include <catch2/catch_test_macros.hpp>

#include <any>

using namespace systems;
using namespace systems::feature;

namespace {
HandlerMetaData handlerMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "ExtrudeFace";
    meta_data.display_name = "拉伸面为实体";
    return meta_data;
}

//! @brief 临时模型承载一个长方体几何组件，并建好几何索引
Index addBoxGeometryComponent(ModelLayer& model_layer)
{
    const Index model_id = model_layer.addModel("temp_Fixture", {});
    auto model_operator = model_layer.getModelOperator(model_id);
    REQUIRE(model_operator.has_value());
    auto geometry = std::make_unique<GeometryData>();
    geometry->setRootShape(GeometryBuilder::makeBox(0.0, 0.0, 0.0, 10.0, 10.0, 10.0));
    auto component = std::make_unique<ComponentData>();
    component->name = "Fixture";
    component->geometry = std::move(geometry);
    const Index component_id = model_operator->addGeometryComponent(std::move(component));
    model_layer.findComponent(component_id)->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    return component_id;
}
}

TEST_CASE("ExtrudeFace execute appends extruded solid to source component", "[ExtrudeFacePlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);

    const Index component_id = addBoxGeometryComponent(model_layer);
    feature_system.setActiveComponentProvider([component_id]() { return std::optional<Index> { component_id }; });

    FeatureSystem::SystemHandlerPtr handler { new ExtrudeFaceHandler };
    REQUIRE(feature_system.registerHandler(handlerMetaData(), std::move(handler)));

    // 未选择截面时返回界面提示语
    const std::any hint = feature_system.invoke("ExtrudeFace");
    REQUIRE(std::any_cast<std::string>(hint) == "请选择一个几何面。");

    // 选择长方体顶面（沿 Z 正向拉伸，长度取默认 10）
    auto* component = model_layer.findComponent(component_id);
    const auto& face_ids = component->geometry->index.face_local_to_global;
    REQUIRE(face_ids.size() == 6 + 1); // 局部 id 从 1 起，0 号为保留槽
    auto selection = std::make_shared<Selection>();
    selection->type = ElementEnum::GeometryFace;
    selection->ids = { face_ids[1] }; // 下标从 1 起，0 号为保留槽
    selection->component_id = component_id;
    REQUIRE(feature_system.setParameter("ExtrudeFace", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(selection)));

    const Index result_component_id = std::any_cast<Index>(feature_system.invoke("ExtrudeFace"));
    REQUIRE(result_component_id == component_id); // 写回源组件

    // 源面保留，拉伸结果以截面副本追加（拓扑独立）：6 源面 + 6 棱柱面，2 个实体
    component->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    REQUIRE(component->geometry->index.solid_local_to_global.size() == 1 + 1 + 1);
    REQUIRE(component->geometry->index.face_local_to_global.size() == 12 + 1);
}
