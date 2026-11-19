/** @file TestScalarRenderRange.cpp
 * @brief 验证独立范围、完整值域与单元筛选的组合行为。
 */
#include "MakeMeshData.h"
#include "MakeMeshDataVtk.h"
#include "MeshPipelineData.h"
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <stdexcept>
#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkIdTypeArray.h>
#include <vtkPlane.h>
#include <vtkPolyData.h>
#include <vtkUnstructuredGrid.h>

namespace {
/** @brief 为各实体域增加有序单值属性，检查闭区间与原始身份。 */
MeshData scalarMesh()
{
    auto mesh = MakeMeshData();
    auto fill_values = [](auto& values, size_t size) {
        for (size_t id = 0; id < size; ++id)
            values.push_back(static_cast<double>(id));
    };
    fill_values(mesh.vertex_attributes_["point_value"], mesh.vertex_positions_.size());
    fill_values(mesh.edge_attributes_["edge_value"], mesh.edge_vertices_.size() / 2);
    fill_values(mesh.face_attributes_["value"], mesh.face_vertices_offset_.size() - 1);
    fill_values(mesh.solid_attributes_["solid_value"], mesh.solid_types_.size());
    return mesh;
}
}

TEST_CASE("Scalar range filtering preserves original IDs and manual hiding", "[scalar-range]")
{
    auto mesh = scalarMesh();
    MeshPipelineData pipeline;
    pipeline.loadModelData(MakeMeshDataVtk(mesh));
    const auto original = pipeline.createSnapshot();
    const auto domain = pipeline.scalarRange("value");
    REQUIRE(domain);
    pipeline.hideFaces({ 2 });
    // 无面筛选和仅筛选边时，面仍使用原始下标；快速路径必须保留人工隐藏。
    const auto manual = pipeline.createSnapshot();
    auto* manual_ids = vtkIdTypeArray::SafeDownCast(manual->visible_faces->GetCellData()->GetArray("PrecessFaceIds"));
    REQUIRE(manual_ids);
    CHECK(manual_ids->LookupValue(2) == -1);
    pipeline.setScalarConfig(ScalarDisplayConfig { "edge_value", { }, std::array { 1.0, 1.0 } });
    const auto edge_filtered = pipeline.createSnapshot();
    CHECK(edge_filtered->visible_faces->GetNumberOfCells() == original->visible_faces->GetNumberOfCells() - 1);
    auto* edge_face_ids = vtkIdTypeArray::SafeDownCast(edge_filtered->visible_faces->GetCellData()->GetArray("PrecessFaceIds"));
    REQUIRE(edge_face_ids);
    CHECK(edge_face_ids->LookupValue(2) == -1);
    pipeline.setScalarConfig(ScalarDisplayConfig { "value", { }, std::array { 1.0, 3.0 } });
    const auto filtered = pipeline.createSnapshot();
    auto* ids = vtkIdTypeArray::SafeDownCast(filtered->visible_faces->GetCellData()->GetArray("PrecessFaceIds"));
    REQUIRE(ids);
    REQUIRE(ids->GetNumberOfValues() == 2);
    CHECK(ids->GetValue(0) == 1);
    CHECK(ids->GetValue(1) == 3);
    CHECK(filtered->visible_edges->GetNumberOfCells() == original->visible_edges->GetNumberOfCells());
    CHECK(filtered->solid_surface->GetNumberOfCells() == original->solid_surface->GetNumberOfCells());
    CHECK(pipeline.scalarRange("value") == domain);
    auto config = *filtered->scalar_config;
    config.display.reset();
    pipeline.setScalarConfig(config);
    CHECK(pipeline.createSnapshot()->scalar_config->mapping == filtered->scalar_config->mapping);
    CHECK(pipeline.createSnapshot()->visible_faces->GetNumberOfCells() == original->visible_faces->GetNumberOfCells() - 1);
    CHECK(filtered->visible_faces->GetNumberOfCells() == 2);
}

TEST_CASE("Scalar range filters edge and solid cells but rejects points", "[scalar-range]")
{
    auto mesh = scalarMesh();
    MeshPipelineData pipeline;
    pipeline.loadModelData(MakeMeshDataVtk(mesh));
    const auto original = pipeline.createSnapshot();
    pipeline.setScalarConfig(ScalarDisplayConfig { "edge_value", { }, std::array { 1.0, 1.0 } });
    const auto edge_filtered = pipeline.createSnapshot();
    REQUIRE(edge_filtered->visible_edges->GetNumberOfCells() == 1);
    auto* edge_ids = vtkIdTypeArray::SafeDownCast(edge_filtered->visible_edges->GetCellData()->GetArray("vtkOriginalCellIds"));
    REQUIRE(edge_ids);
    CHECK(edge_ids->GetValue(0) == 1);
    pipeline.setScalarConfig(ScalarDisplayConfig { "solid_value", { }, std::array { 0.0, 0.0 } });
    const auto filtered = pipeline.createSnapshot();
    CHECK(filtered->solid_surface->GetNumberOfCells() > 0);
    CHECK(filtered->solid_surface->GetNumberOfCells() < original->solid_surface->GetNumberOfCells());
    auto* solid_ids = vtkIdTypeArray::SafeDownCast(filtered->solid_surface->GetCellData()->GetArray("vtkOriginalCellIds"));
    REQUIRE(solid_ids);
    REQUIRE(solid_ids->GetNumberOfValues() == filtered->solid_surface->GetNumberOfCells());
    for (vtkIdType id = 0; id < solid_ids->GetNumberOfValues(); ++id)
        CHECK(solid_ids->GetValue(id) == 0);
    auto* values = filtered->solid_surface->GetCellData()->GetArray("solid_value");
    REQUIRE(values);
    for (vtkIdType id = 0; id < values->GetNumberOfTuples(); ++id)
        CHECK(values->GetComponent(id, 0) == 0.0);
    // 沿用 develop 的完整体内部线框语义，范围筛选仅改变体表面，不重建内部边线缓存。
    CHECK(filtered->solid_internal_edges == original->solid_internal_edges);
    REQUIRE(filtered->solid_internal_edges->GetCellData()->GetArray("solid_value"));
    CHECK(filtered->visible_faces->GetNumberOfCells() == original->visible_faces->GetNumberOfCells());
    CHECK_THROWS_AS(pipeline.setScalarConfig(ScalarDisplayConfig { "point_value", { }, std::array { 0.0, 1.0 } }), std::invalid_argument);
}

TEST_CASE("Scalar range full values survive empty results missing values and clipping", "[scalar-range]")
{
    auto mesh = scalarMesh();
    mesh.face_attributes_["value"][0] = std::numeric_limits<double>::quiet_NaN();
    MeshPipelineData pipeline;
    pipeline.loadModelData(MakeMeshDataVtk(mesh));
    const auto original = pipeline.createSnapshot();
    const auto range = pipeline.scalarRange("value");
    REQUIRE(range);
    CHECK((*range)[0] == 1.0);
    pipeline.setScalarConfig(ScalarDisplayConfig { "value", { }, std::array { 100.0, 200.0 } });
    CHECK(pipeline.createSnapshot()->visible_faces->GetNumberOfCells() == 0);
    vtkNew<vtkPlane> plane;
    plane->SetOrigin(100.0, 0.0, 0.0);
    plane->SetNormal(1.0, 0.0, 0.0);
    pipeline.setClipPlane(plane);
    CHECK(pipeline.scalarRange("value") == range);
    auto config = *pipeline.createSnapshot()->scalar_config;
    config.display.reset();
    pipeline.setScalarConfig(config);
    pipeline.setClipPlane(nullptr);
    CHECK(pipeline.createSnapshot()->visible_faces->GetNumberOfCells() == original->visible_faces->GetNumberOfCells());
    mesh.face_attributes_["constant"].assign(mesh.face_vertices_offset_.size() - 1, 0.5);
    pipeline.loadModelData(MakeMeshDataVtk(mesh));
    const auto constant = pipeline.scalarRange("constant");
    REQUIRE(constant);
    CHECK((*constant)[0] == 0.5);
    CHECK((*constant)[1] == 0.5);
}

TEST_CASE("Scalar config updates report mesh output rebuilds and retain the latest completed configuration", "[scalar-range]")
{
    auto mesh = scalarMesh();
    MeshPipelineData pipeline;
    pipeline.loadModelData(MakeMeshDataVtk(mesh));
    const auto original = pipeline.createSnapshot();
    REQUIRE(original->mesh_outputs_rebuilt);
    ScalarDisplayConfig config { "value", std::array { -1.0, 4.0 }, std::array { 1.0, 3.0 } };
    pipeline.setScalarConfig(config);
    const auto filtered = pipeline.createSnapshot();
    REQUIRE(filtered->mesh_outputs_rebuilt);

    // 仅修改映射或重复提交相同筛选时，共享已完成的网格输出，不使选择失效。
    config.mapping = std::array { -2.0, 6.0 };
    pipeline.setScalarConfig(config);
    const auto mapped = pipeline.createSnapshot();
    CHECK_FALSE(mapped->mesh_outputs_rebuilt);
    CHECK(filtered->mesh_outputs_rebuilt); // 新快照的标记不能改写已发布快照。
    CHECK(mapped->visible_faces == filtered->visible_faces);
    CHECK(mapped->visible_edges == filtered->visible_edges);
    CHECK(mapped->solid_surface == filtered->solid_surface);
    REQUIRE(mapped->scalar_config);
    CHECK(mapped->scalar_config->mapping == config.mapping);
    CHECK(filtered->scalar_config->mapping != config.mapping);
    pipeline.setScalarConfig(config);
    const auto repeated = pipeline.createSnapshot();
    CHECK_FALSE(repeated->mesh_outputs_rebuilt);
    CHECK(repeated->visible_faces == mapped->visible_faces);

    // 非法请求不改变已接受的配置，也不触发网格输出重建。
    config.display = std::array { std::numeric_limits<double>::quiet_NaN(), 3.0 };
    CHECK_THROWS_AS(pipeline.setScalarConfig(config), std::invalid_argument);
    const auto rejected = pipeline.createSnapshot();
    CHECK_FALSE(rejected->mesh_outputs_rebuilt);
    REQUIRE(rejected->scalar_config);
    CHECK(rejected->scalar_config->display == filtered->scalar_config->display);

    // 区间改变会重新生成网格输出；即使仍保留原来的三个单元，也不再比较集合来保留选择。
    config.display = std::array { 0.9, 3.1 };
    pipeline.setScalarConfig(config);
    const auto same_cells = pipeline.createSnapshot();
    CHECK(same_cells->mesh_outputs_rebuilt);
    CHECK_FALSE(mapped->mesh_outputs_rebuilt);
    CHECK(same_cells->visible_faces != filtered->visible_faces);
    auto* face_ids = vtkIdTypeArray::SafeDownCast(same_cells->visible_faces->GetCellData()->GetArray("PrecessFaceIds"));
    REQUIRE(face_ids);
    REQUIRE(face_ids->GetNumberOfValues() == 3);
    for (vtkIdType id = 0; id < face_ids->GetNumberOfValues(); ++id)
        CHECK(face_ids->GetValue(id) == id + 1);

    // 取消全范围筛选仍重新生成网格输出，恢复前后单元数相同也应使旧选择失效。
    config.display = pipeline.scalarRange("value");
    pipeline.setScalarConfig(config);
    const auto full_range = pipeline.createSnapshot();
    REQUIRE(full_range->mesh_outputs_rebuilt);
    CHECK(full_range->visible_faces->GetNumberOfCells() == original->visible_faces->GetNumberOfCells());
    pipeline.setScalarConfig(std::nullopt);
    const auto canceled = pipeline.createSnapshot();
    CHECK(canceled->mesh_outputs_rebuilt);
    CHECK(canceled->visible_faces->GetNumberOfCells() == full_range->visible_faces->GetNumberOfCells());
    CHECK_FALSE(canceled->scalar_config);

    // 无筛选的着色与取消只更新完成配置，继续共享网格输出。
    config.display.reset();
    pipeline.setScalarConfig(config);
    const auto unfiltered = pipeline.createSnapshot();
    CHECK_FALSE(unfiltered->mesh_outputs_rebuilt);
    REQUIRE(unfiltered->scalar_config);
    CHECK(unfiltered->visible_faces == canceled->visible_faces);
    pipeline.setScalarConfig(std::nullopt);
    const auto unfiltered_canceled = pipeline.createSnapshot();
    CHECK_FALSE(unfiltered_canceled->mesh_outputs_rebuilt);
    CHECK(unfiltered_canceled->visible_faces == unfiltered->visible_faces);
    CHECK_FALSE(unfiltered_canceled->scalar_config);
    CHECK(pipeline.createSnapshot()->scalar_config == std::nullopt);
}

TEST_CASE("Source reload retains applicable scalar configuration and removes only invalid parts", "[scalar-range]")
{
    auto mesh = scalarMesh();
    MeshPipelineData pipeline;
    pipeline.loadModelData(MakeMeshDataVtk(mesh));
    pipeline.setScalarConfig(ScalarDisplayConfig { "value", { }, std::array { 1.0, 3.0 } });
    const auto before = pipeline.createSnapshot();
    pipeline.setVisibility(false);
    pipeline.hideFaces({ 1 });
    pipeline.loadModelData(MakeMeshDataVtk(mesh));
    const auto reloaded = pipeline.createSnapshot();
    CHECK(reloaded->mesh_outputs_rebuilt);
    CHECK(reloaded->generation != before->generation);
    REQUIRE(reloaded->scalar_config);
    CHECK(reloaded->scalar_config->mapping == before->scalar_config->mapping);
    CHECK(reloaded->visible_faces->GetNumberOfCells() == 3);
    CHECK_FALSE(reloaded->visible);
    // 全缺失值只能解除筛选，属性仍可按原有 NaN 规则着色。
    mesh.face_attributes_["value"].assign(mesh.face_attributes_["value"].size(), std::numeric_limits<double>::quiet_NaN());
    pipeline.loadModelData(MakeMeshDataVtk(mesh));
    const auto missing = pipeline.createSnapshot();
    REQUIRE(missing->scalar_config);
    CHECK_FALSE(missing->scalar_config->display);
    CHECK(missing->scalar_config->mapping == before->scalar_config->mapping);
    // 更高优先级点属性出现后，保留映射但解除不适用的单元筛选。
    mesh.face_attributes_["value"].assign(mesh.face_attributes_["value"].size(), 2.0);
    pipeline.loadModelData(MakeMeshDataVtk(mesh));
    pipeline.setScalarConfig(ScalarDisplayConfig { "value", std::array { -4.0, 4.0 }, std::array { 1.0, 3.0 } });
    mesh.vertex_attributes_["value"].assign(mesh.vertex_positions_.size(), 10.0);
    pipeline.loadModelData(MakeMeshDataVtk(mesh));
    const auto point_shadowed = pipeline.createSnapshot();
    REQUIRE(point_shadowed->scalar_config);
    CHECK_FALSE(point_shadowed->scalar_config->display);
    CHECK(point_shadowed->scalar_config->mapping == std::optional(std::array { -4.0, 4.0 }));
    CHECK(point_shadowed->visible_faces->GetNumberOfCells() == mesh.face_vertices_offset_.size() - 1);
    // 属性完全消失后清空显示描述，场景重建不能恢复旧着色。
    mesh.vertex_attributes_.erase("value");
    mesh.face_attributes_.erase("value");
    pipeline.loadModelData(MakeMeshDataVtk(mesh));
    CHECK_FALSE(pipeline.createSnapshot()->scalar_config);
}

TEST_CASE("Scalar lookup uses the first named array before checking filtering support", "[scalar-range]")
{
    auto mesh = scalarMesh();
    mesh.vertex_attributes_["value"].assign(mesh.vertex_positions_.size(), 10.0);
    mesh.edge_attributes_["value"].assign(mesh.edge_vertices_.size() / 2, 20.0);
    mesh.solid_attributes_["value"].assign(mesh.solid_types_.size(), 30.0);
    MeshPipelineData pipeline;

    SECTION("point arrays shadow every cell array")
    {
        pipeline.loadModelData(MakeMeshDataVtk(mesh));
        CHECK(pipeline.scalarRange("value") == std::optional(std::array { 10.0, 10.0 }));
        CHECK_THROWS_AS(pipeline.setScalarConfig(ScalarDisplayConfig { "value", { }, std::array { 0.0, 100.0 } }), std::invalid_argument);
        CHECK_NOTHROW(pipeline.setScalarConfig(ScalarDisplayConfig { "value", std::array { 1.0, 2.0 }, { } }));
    }
    SECTION("edge then face then solid arrays follow the same priority")
    {
        mesh.vertex_attributes_.erase("value");
        pipeline.loadModelData(MakeMeshDataVtk(mesh));
        CHECK(pipeline.scalarRange("value") == std::optional(std::array { 20.0, 20.0 }));
        pipeline.setScalarConfig(ScalarDisplayConfig { "value", { }, std::array { 20.0, 20.0 } });
        CHECK(pipeline.createSnapshot()->visible_edges->GetNumberOfCells() == mesh.edge_vertices_.size() / 2);

        mesh.edge_attributes_.erase("value");
        pipeline.loadModelData(MakeMeshDataVtk(mesh));
        CHECK(pipeline.scalarRange("value") == std::optional(std::array { 0.0, static_cast<double>(mesh.face_vertices_offset_.size() - 2) }));
        pipeline.setScalarConfig(ScalarDisplayConfig { "value", { }, std::array { 1.0, 3.0 } });
        CHECK(pipeline.createSnapshot()->visible_faces->GetNumberOfCells() == 3);

        mesh.face_attributes_.erase("value");
        pipeline.loadModelData(MakeMeshDataVtk(mesh));
        CHECK(pipeline.scalarRange("value") == std::optional(std::array { 30.0, 30.0 }));
        pipeline.setScalarConfig(ScalarDisplayConfig { "value", { }, std::array { 30.0, 30.0 } });
        CHECK(pipeline.createSnapshot()->solid_surface->GetNumberOfCells() > 0);
    }
    SECTION("an unsupported first array never falls through to a later scalar array")
    {
        mesh.vertex_attributes_.erase("value");
        mesh.edge_attributes_["value"].resize(mesh.edge_vertices_.size(), 20.0);
        pipeline.loadModelData(MakeMeshDataVtk(mesh));
        CHECK_FALSE(pipeline.scalarRange("value"));
        CHECK_THROWS_AS(pipeline.setScalarConfig(ScalarDisplayConfig { "value", { }, std::array { 1.0, 3.0 } }), std::invalid_argument);
        CHECK_NOTHROW(pipeline.setScalarConfig(ScalarDisplayConfig { "value", std::array { 0.0, 40.0 }, { } }));
    }
}
