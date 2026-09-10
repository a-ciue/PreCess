#include "ComponentData.h"
#include "EventBus.h"
#include "FeatureDemoHandler.h"
#include "FeatureSystem.h"
#include "MeshData.h"
#include "ModelLayer.h"

#include <catch2/catch_test_macros.hpp>

#include <array>

using namespace systems;
using namespace systems::feature;

namespace {
HandlerMetaData demoMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "FeatureDemo";
    meta_data.display_name = "功能示例";
    return meta_data;
}

Index addSingleComponentModel(ModelLayer& model_layer)
{
    auto mesh = std::make_unique<MeshData>();
    mesh->init();
    mesh->vertex_positions_ = { { 1.0, 2.0, 3.0 } };

    auto component = std::make_unique<ComponentData>();
    component->id = -1;
    component->name = "Comp_0";
    component->mesh = std::move(mesh);
    ComponentDatas components;
    components.push_back(std::move(component));

    Index model_id = model_layer.addModel("test_model", std::move(components));
    return model_layer.modelById(model_id)->componentIds().front();
}
}

TEST_CASE("FeatureDemo execute scales active component mesh via context", "[FeatureDemoPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    const Index component_id = addSingleComponentModel(model_layer);

    FeatureSystem::SystemHandlerPtr handler { new FeatureDemoHandler };
    REQUIRE(feature_system.registerHandler(demoMetaData(), std::move(handler)));

    // 注入动态上下文：活动组件 id 由 UI 层（此处为测试）提供
    feature_system.setActiveComponentProvider([component_id]() { return std::optional<Index> { component_id }; });
    REQUIRE(feature_system.setParameter("FeatureDemo", 0, core::ArgObject::create<ArgTypeEnum::Float>(2.0)));
    REQUIRE_NOTHROW(feature_system.invoke("FeatureDemo"));

    const auto* component = model_layer.findComponent(component_id);
    REQUIRE(component != nullptr);
    REQUIRE(component->mesh != nullptr);
    // 顶点坐标常驻组件 MeshData，直接验证就地缩放结果
    REQUIRE(component->mesh->vertex_positions_.size() == 1);
    REQUIRE(component->mesh->vertex_positions_[0] == std::array<double, 3> { 2.0, 4.0, 6.0 });
}
