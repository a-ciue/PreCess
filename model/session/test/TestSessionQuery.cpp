/**
 * @file TestSessionQuery.cpp
 * @brief SessionQuery 原生查询接口测试（几何组件摘要、身份反查、局部 id 解析）
 */
#include "SessionQuery.h"

#include "ComponentData.h"
#include "GeometryBuilder.h"
#include "GeometryData.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "ModelOperator.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <numeric>
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

TEST_CASE("SessionQuery histogram uses original finite values and refreshes with source data", "[session][query][scalar-range][histogram]")
{
    ModelLayer layer;
    SessionQuery query(layer);
    auto component = std::make_unique<ComponentData>();
    component->mesh = std::make_unique<MeshData>();
    auto& mesh = *component->mesh;
    mesh.vertex_positions_ = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
    mesh.vertex_count_ = 4;
    mesh.face_vertices_ = { 0, 1, 2, 0, 1, 3, 0, 2, 3, 1, 2, 3, 0, 1, 2 };
    mesh.face_vertices_offset_ = { 0, 3, 6, 9, 12, 15 };
    mesh.edge_vertices_ = { 0, 1, 1, 2 };
    mesh.solid_types_ = { 10 };
    mesh.solid_vertices_ = { 0, 1, 2, 3 };
    mesh.solid_vertices_offset_ = { 0, 4 };
    mesh.vertex_attributes_["point_value"] = { 0, 1, 2, 3 };
    mesh.edge_attributes_["edge_value"] = { 0, 1 };
    mesh.solid_attributes_["solid_value"] = { 0 };
    mesh.face_attributes_["vector"] = std::vector<double>(15, 1.0);
    ComponentDatas components;
    components.push_back(std::move(component));
    const auto model_id = layer.addModel("histogram", std::move(components));
    const auto component_id = layer.modelById(model_id)->componentIds().front();
    mesh.face_attributes_["value"] = { -1.0, 0.0, 1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity() };
    const auto histogram = query.scalarHistogram(component_id, "value");
    REQUIRE(histogram);
    const auto& counts = histogram->counts;
    REQUIRE(counts.size() == 32);
    CHECK(std::accumulate(counts.begin(), counts.end(), std::uint64_t { 0 }) == 3);
    CHECK(counts.front() == 1);
    CHECK(counts[16] == 1);
    CHECK(counts.back() == 1);
    // 边界前后相邻的双精度值按实际提示边界归箱，最大值包含在最后一箱。
    const double boundary = 1.0 / 32;
    mesh.face_attributes_["value"] = { 0.0, std::nextafter(boundary, 0.0), boundary, std::nextafter(boundary, 1.0), 1.0 };
    const auto boundary_histogram = query.scalarHistogram(component_id, "value");
    REQUIRE(boundary_histogram);
    CHECK(boundary_histogram->edges[1] == boundary);
    CHECK(boundary_histogram->counts.front() == 2);
    CHECK(boundary_histogram->counts[1] == 2);
    CHECK(boundary_histogram->edges.back() == 1.0);
    CHECK(boundary_histogram->counts.back() == 1);

    // 两端保留实际极值，首尾和内部使用同一箱宽。
    mesh.face_attributes_["value"] = { 0.0123, 0.02, 0.07, 0.1, 0.1234 };
    const auto updated = query.scalarHistogram(component_id, "value");
    REQUIRE(updated);
    REQUIRE(updated->edges.size() == 33);
    CHECK(updated->edges.front() == 0.0123);
    CHECK(updated->edges.back() == 0.1234);
    const double width = (updated->edges.back() - updated->edges.front()) / 32;
    for (size_t i = 1; i < updated->edges.size(); ++i)
        CHECK(std::abs(updated->edges[i] - updated->edges[i - 1] - width) < 1e-16);
    CHECK(std::accumulate(updated->counts.begin(), updated->counts.end(), std::uint64_t { 0 }) == 5);
    // 点、边、体同样统计原始数组，不限于质量指标或可筛选的实体域。
    for (const auto* attribute : { "point_value", "edge_value", "solid_value" }) {
        const auto histogram = query.scalarHistogram(component_id, attribute);
        REQUIRE(histogram);
        CHECK(std::accumulate(histogram->counts.begin(), histogram->counts.end(), std::uint64_t { 0 }) > 0);
    }
    CHECK_FALSE(query.scalarHistogram(component_id, "missing"));
    CHECK_FALSE(query.scalarHistogram(component_id, "vector"));
    // 大数值窄区间保留分箱精度，跨正负极值时仍能避免差值溢出。
    const double largest = std::numeric_limits<double>::max();
    for (const auto& values : { std::vector { 1e16, 1e16 + 2, 1e16 + 4, 1e16 + 6, 1e16 + 8 },
             std::vector { -largest, -largest / 2, 0.0, largest / 2, largest } }) {
        mesh.face_attributes_["value"] = values;
        std::vector<std::uint64_t> expected(32, 0);
        for (const auto bin : { 0, 8, 16, 24, 31 })
            expected[bin] = 1;
        const auto result = query.scalarHistogram(component_id, "value");
        REQUIRE(result);
        CHECK(result->counts == expected);
        CHECK(result->origin.has_value() == (values.front() == 1e16));
        if (result->origin)
            CHECK(result->edges[1] == 0.25);
    }
    const double tiny = std::numeric_limits<double>::denorm_min();
    mesh.face_attributes_["value"] = { 0.0, 0.0, tiny, tiny, tiny };
    const auto subnormal = query.scalarHistogram(component_id, "value");
    REQUIRE(subnormal);
    CHECK(subnormal->edges == std::vector<double> { 0.0, tiny });
    CHECK(subnormal->counts == std::vector<std::uint64_t> { 5 });

    // 源数据更新后重新统计；常量只返回一箱，全部缺失则没有可显示的分布。
    mesh.face_attributes_["value"].assign(5, -7.12345);
    const auto constant = query.scalarHistogram(component_id, "value");
    REQUIRE(constant);
    CHECK(constant->edges == std::vector<double> { -7.12345, -7.12345 });
    CHECK(constant->counts == std::vector<std::uint64_t> { 5 });
    mesh.face_attributes_["value"].assign(5, std::numeric_limits<double>::quiet_NaN());
    CHECK_FALSE(query.scalarHistogram(component_id, "value"));
    CHECK_FALSE(query.scalarHistogram(-1, "value"));

    // 同名查找仅跳过未生成渲染数组的条目；有效多分量或全缺失的点属性仍优先。
    mesh.face_attributes_["value"] = { 0, 0, 1, 1, 1 };
    for (const auto& invalid : { std::vector<double> { }, std::vector<double> { 1 } }) {
        mesh.vertex_attributes_["value"] = invalid;
        const auto fallback = query.scalarHistogram(component_id, "value");
        REQUIRE(fallback);
        CHECK(fallback->counts.front() == 2);
        CHECK(fallback->counts.back() == 3);
    }
    mesh.vertex_attributes_["value"].assign(12, 1.0);
    CHECK_FALSE(query.scalarHistogram(component_id, "value"));
    mesh.vertex_attributes_["value"].assign(4, std::numeric_limits<double>::quiet_NaN());
    CHECK_FALSE(query.scalarHistogram(component_id, "value"));
    mesh.vertex_attributes_["value"].assign(4, 2.0);
    const auto point = query.scalarHistogram(component_id, "value");
    REQUIRE(point);
    CHECK(point->edges == std::vector<double> { 2.0, 2.0 });
    CHECK(point->counts == std::vector<std::uint64_t> { 4 });
}
