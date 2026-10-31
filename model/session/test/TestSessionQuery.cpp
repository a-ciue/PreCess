/**
 * @file TestSessionQuery.cpp
 * @brief SessionQuery 原生查询接口测试（几何组件摘要、身份反查、局部 id 解析）
 */
#include "SessionQuery.h"

#include "ComponentData.h"
#include "GeometryBuilder.h"
#include "GeometryData.h"
#include "ModelLayer.h"
#include "ModelOperator.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace session;

namespace {
// 造一个含单个几何组件（长方体 1x2x3）的模型，返回组件 id
Index makeGeometryComponent(ModelLayer& layer, Index model_id, const std::string& name)
{
    auto geometry = std::make_unique<GeometryData>();
    geometry->setRootShape(GeometryBuilder::makeBox(0.0, 0.0, 0.0, 1.0, 2.0, 3.0));
    auto component = std::make_unique<ComponentData>();
    component->name = name;
    component->geometry = std::move(geometry);
    auto op = layer.getModelOperator(model_id);
    return op->addGeometryComponent(std::move(component));
}
}

TEST_CASE("SessionQuery model and component summaries", "[session][query]")
{
    ModelLayer layer;
    SessionQuery query(layer);

    const Index model_a = layer.addModel("ModelA", { });
    const Index model_b = layer.addModel("ModelB", { });

    auto models = query.listModels();
    REQUIRE(models.size() == 2);
    // unordered_map 遍历序不定，按 id 查找断言
    auto by_id = [&models](Index id) -> const ModelSummary* {
        for (const auto& m : models)
            if (m.model_id == id)
                return &m;
        return nullptr;
    };
    const ModelSummary* summary_a = by_id(model_a);
    const ModelSummary* summary_b = by_id(model_b);
    REQUIRE(summary_a != nullptr);
    REQUIRE(summary_b != nullptr);
    CHECK(summary_a->name == "ModelA");
    CHECK(summary_a->component_count == 0);
    CHECK(summary_b->name == "ModelB");

    SECTION("空模型无组件摘要")
    {
        CHECK(query.componentSummaries(model_a).empty());
        CHECK(query.componentIds(model_a).empty());
        CHECK(query.meshSummary(model_a).has_mesh == false);
    }

    SECTION("几何组件摘要与身份反查")
    {
        const Index comp_id = makeGeometryComponent(layer, model_a, "Box");

        auto summaries = query.componentSummaries(model_a);
        REQUIRE(summaries.size() == 1);
        CHECK(summaries[0].component_id == comp_id);
        CHECK(summaries[0].name == "Box");
        CHECK(summaries[0].has_mesh == false);
        CHECK(summaries[0].has_geometry == true);

        CHECK(query.findModelIdByComponent(comp_id) == model_a);
        CHECK(query.findModelIdByComponent(-1) == -1);
        CHECK(query.hasModel(model_a));
        CHECK(query.hasComponent(comp_id));
        CHECK(query.hasModel(9999) == false);
        CHECK(query.hasComponent(9999) == false);

        REQUIRE(query.modelName(model_a).has_value());
        CHECK(*query.modelName(model_a) == "ModelA");
        CHECK(query.modelName(9999).has_value() == false);
        REQUIRE(query.componentName(comp_id).has_value());
        CHECK(*query.componentName(comp_id) == "Box");
    }
}

TEST_CASE("SessionQuery geometry summary and local id resolution", "[session][query][geometry]")
{
    ModelLayer layer;
    SessionQuery query(layer);
    const Index model_id = layer.addModel("Geom", { });
    const Index comp_id = makeGeometryComponent(layer, model_id, "Box");

    // 长方体拓扑：8 顶点 / 12 边 / 6 面 / 1 实体
    auto summary = query.geometrySummary(comp_id);
    CHECK(summary.has_geometry);
    CHECK(summary.vertex_count == 8);
    CHECK(summary.edge_count == 12);
    CHECK(summary.face_count == 6);
    CHECK(summary.solid_count == 1);

    CHECK(query.geometrySummary(-1).has_geometry == false);

    SECTION("几何局部 id 反查全局 id")
    {
        // 局部 id 为 OCCT 拓扑枚举序号（1 基，0 与 count+1 无效）
        for (int local = 1; local <= 6; ++local) {
            auto gid = query.resolveGeometryFaceLocalId(comp_id, local);
            REQUIRE(gid.has_value());
            CHECK(*gid >= 0);
        }
        CHECK(query.resolveGeometryFaceLocalId(comp_id, 0).has_value() == false);
        CHECK(query.resolveGeometryFaceLocalId(comp_id, 7).has_value() == false);
        CHECK(query.resolveGeometryFaceLocalId(-1, 1).has_value() == false);

        auto edge = query.resolveGeometryEdgeLocalId(comp_id, 1);
        REQUIRE(edge.has_value());
        CHECK(query.resolveGeometryEdgeLocalId(comp_id, 13).has_value() == false);

        auto vertex = query.resolveGeometryVertexLocalId(comp_id, 1);
        REQUIRE(vertex.has_value());
        CHECK(query.resolveGeometryVertexLocalId(comp_id, 9).has_value() == false);

        auto solid = query.resolveGeometrySolidLocalId(comp_id, 1);
        REQUIRE(solid.has_value());
        CHECK(query.resolveGeometrySolidLocalId(comp_id, 2).has_value() == false);
    }

    SECTION("无网格组件的查询路径")
    {
        CHECK(query.meshDataByComponent(comp_id).has_value() == false);
        CHECK(query.pointGlobalId(comp_id, 0) == -1);
        CHECK(query.findEdgeByEndpoints(comp_id, 0, 1).has_value() == false);
        CHECK(query.componentAttributeInfos(comp_id).empty());
        CHECK(query.firstMeshComponentId(model_id).has_value() == false);
    }
}

TEST_CASE("SessionQuery render data views", "[session][query]")
{
    ModelLayer layer;
    SessionQuery query(layer);
    const Index model_id = layer.addModel("Geom", { });
    const Index comp_id = makeGeometryComponent(layer, model_id, "Box");

    auto geometries = query.geometryDataByModel(model_id);
    REQUIRE(geometries.size() == 1);
    CHECK(geometries[0].component_id == comp_id);
    CHECK(&geometries[0].shape == layer.findComponent(comp_id)->geometry->rootShape.get());

    auto by_component = query.geometryDataByComponent(comp_id);
    REQUIRE(by_component.has_value());
    CHECK(by_component->component_id == comp_id);

    CHECK(query.geometryDataByComponent(-1).has_value() == false);
    CHECK(query.meshDataByComponent(-1).has_value() == false);
}
