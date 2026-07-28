#include "ArgObject.h"
#include "ComponentData.h"
#include "EventBus.h"
#include "FeatureEvents.h"
#include "FeatureSystem.h"
#include "MeshData.h"
#include "MeshQualityHandler.h"
#include "ModelLayer.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <any>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace systems;
using namespace systems::feature;

namespace {

/**
 * @brief 创建同时包含理想三角形和理想四面体的单组件模型
 */
Index addQualityTestComponent(ModelLayer& model_layer)
{
    auto mesh = std::make_unique<MeshData>();
    mesh->init();

    const double sqrt_three = std::sqrt(3.0);
    mesh->vertex_positions_ = {
        { 0.0, 0.0, 0.0 },
        { 1.0, 0.0, 0.0 },
        { 0.5, sqrt_three / 2.0, 0.0 },
        { 0.5, sqrt_three / 6.0, std::sqrt(2.0 / 3.0) },
    };
    mesh->face_vertices_ = { 0, 1, 2 };
    mesh->face_vertices_offset_ = { 0, 3 };
    mesh->solid_types_ = { 10 }; // VTK_TETRA
    mesh->solid_vertices_ = { 0, 1, 2, 3 };
    mesh->solid_vertices_offset_ = { 0, 4 };

    auto component = std::make_unique<ComponentData>();
    component->name = "QualityComponent";
    component->mesh = std::move(mesh);
    ComponentDatas components;
    components.push_back(std::move(component));

    const Index model_id = model_layer.addModel("QualityModel", std::move(components));
    return model_layer.modelById(model_id)->componentIds().front();
}

/**
 * @brief 创建测试使用的 Feature 元数据
 */
HandlerMetaData qualityMetaData()
{
    HandlerMetaData metadata;
    metadata.name = "MeshQuality";
    metadata.display_name = "网格质量";
    return metadata;
}

}

TEST_CASE("MeshQuality computes scalar attributes", "[MeshQualityPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    const Index component_id = addQualityTestComponent(model_layer);
    std::vector<std::string> display_requests;
    auto display_subscription = bus.subscribe<ScalarAttributeDisplayRequestedEvent>(
        [&display_requests](const ScalarAttributeDisplayRequestedEvent& event) {
            display_requests.push_back(event.attribute_name);
        });
    REQUIRE(static_cast<bool>(display_subscription));

    FeatureSystem::SystemHandlerPtr handler { new MeshQualityHandler };
    REQUIRE(feature_system.registerHandler(qualityMetaData(), std::move(handler)));
    feature_system.setActiveComponentProvider(
        [component_id]() { return std::optional<Index> { component_id }; });

    const std::any execution_result = feature_system.invoke("MeshQuality");
    REQUIRE(execution_result.type() == typeid(std::string));
    REQUIRE(std::any_cast<const std::string&>(execution_result).find("Scaled Jacobian") != std::string::npos);
    REQUIRE(display_requests == std::vector<std::string> { "s_mesh_quality_scaled_jacobian_1" });

    const ComponentData* component = model_layer.findComponent(component_id);
    REQUIRE(component != nullptr);
    REQUIRE(component->mesh != nullptr);

    const auto& face_quality = component->mesh->face_attributes_.at("f_mesh_quality_scaled_jacobian_1");
    const auto& solid_quality = component->mesh->solid_attributes_.at("s_mesh_quality_scaled_jacobian_1");
    REQUIRE(face_quality.size() == 1);
    REQUIRE(solid_quality.size() == 1);
    REQUIRE(face_quality.front() == Catch::Approx(1.0));
    REQUIRE(solid_quality.front() == Catch::Approx(1.0));

    // 切换指标后保留之前的质量属性，方便用户在属性列表中比较不同指标。
    REQUIRE(feature_system.setParameter(
        "MeshQuality", 0, core::ArgObject::create<ArgTypeEnum::Combo>(1)));
    const std::any skew_result = feature_system.invoke("MeshQuality");
    REQUIRE(skew_result.type() == typeid(std::string));
    REQUIRE(display_requests.back() == "s_mesh_quality_equiangle_skew_1");
    REQUIRE(display_requests.size() == 2);
    REQUIRE(component->mesh->face_attributes_.count("f_mesh_quality_scaled_jacobian_1") == 1);
    REQUIRE(component->mesh->solid_attributes_.count("s_mesh_quality_scaled_jacobian_1") == 1);
    REQUIRE(component->mesh->face_attributes_.count("f_mesh_quality_equiangle_skew_1") == 1);
    REQUIRE(component->mesh->solid_attributes_.count("s_mesh_quality_equiangle_skew_1") == 1);
}

TEST_CASE("MeshQuality exposes metric selection parameter", "[MeshQualityPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);

    FeatureSystem::SystemHandlerPtr handler { new MeshQualityHandler };
    REQUIRE(feature_system.registerHandler(qualityMetaData(), std::move(handler)));

    const auto infos = feature_system.getFeatureInfos();
    REQUIRE(infos.size() == 1);
    REQUIRE(infos.front()->arg_types.size() == 1);
    REQUIRE(infos.front()->arg_types.front().type == ArgTypeEnum::Combo);
    REQUIRE(infos.front()->arg_types.front().content.find("Tet Collapse") != std::string::npos);
}
