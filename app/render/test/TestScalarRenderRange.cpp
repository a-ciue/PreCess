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
    fill_values(mesh.vertex_attributes_["value"], mesh.vertex_positions_.size());
    fill_values(mesh.edge_attributes_["value"], mesh.edge_vertices_.size() / 2);
    fill_values(mesh.face_attributes_["value"], mesh.face_vertices_offset_.size() - 1);
    fill_values(mesh.solid_attributes_["value"], mesh.solid_types_.size());
    return mesh;
}
}

TEST_CASE("Scalar range filtering preserves original IDs and manual hiding", "[scalar-range]")
{
    auto mesh = scalarMesh();
    MeshPipelineData pipeline;
    pipeline.loadModelData(MakeMeshDataVtk(mesh));
    const auto original = pipeline.createSnapshot();
    const auto domain = pipeline.scalarRange("value", ElementType::FACE);
    REQUIRE(domain);
    pipeline.hideFaces({ 2 });
    // 无面筛选和仅筛选边时，面仍使用原始下标；快速路径必须保留人工隐藏。
    const auto manual = pipeline.createSnapshot();
    auto* manual_ids = vtkIdTypeArray::SafeDownCast(manual->visible_faces->GetCellData()->GetArray("PrecessFaceIds"));
    REQUIRE(manual_ids);
    CHECK(manual_ids->LookupValue(2) == -1);
    pipeline.setScalarConfig(ScalarDisplayConfig { "value", ElementType::EDGE, { }, std::array { 1.0, 1.0 } });
    const auto edge_filtered = pipeline.createSnapshot();
    CHECK(edge_filtered->visible_faces->GetNumberOfCells() == original->visible_faces->GetNumberOfCells() - 1);
    auto* edge_face_ids = vtkIdTypeArray::SafeDownCast(edge_filtered->visible_faces->GetCellData()->GetArray("PrecessFaceIds"));
    REQUIRE(edge_face_ids);
    CHECK(edge_face_ids->LookupValue(2) == -1);
    pipeline.setScalarConfig(ScalarDisplayConfig { "value", ElementType::FACE, { }, std::array { 1.0, 3.0 } });
    const auto filtered = pipeline.createSnapshot();
    auto* ids = vtkIdTypeArray::SafeDownCast(filtered->visible_faces->GetCellData()->GetArray("PrecessFaceIds"));
    REQUIRE(ids);
    REQUIRE(ids->GetNumberOfValues() == 2);
    CHECK(ids->GetValue(0) == 1);
    CHECK(ids->GetValue(1) == 3);
    CHECK(filtered->visible_edges->GetNumberOfCells() == original->visible_edges->GetNumberOfCells());
    CHECK(filtered->solid_surface->GetNumberOfCells() == original->solid_surface->GetNumberOfCells());
    CHECK(pipeline.scalarRange("value", ElementType::FACE) == domain);
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
    pipeline.setScalarConfig(ScalarDisplayConfig { "value", ElementType::EDGE, { }, std::array { 1.0, 1.0 } });
    const auto edge_filtered = pipeline.createSnapshot();
    REQUIRE(edge_filtered->visible_edges->GetNumberOfCells() == 1);
    auto* edge_ids = vtkIdTypeArray::SafeDownCast(edge_filtered->visible_edges->GetCellData()->GetArray("vtkOriginalCellIds"));
    REQUIRE(edge_ids);
    CHECK(edge_ids->GetValue(0) == 1);
    pipeline.setScalarConfig(ScalarDisplayConfig { "value", ElementType::SOLID, { }, std::array { 0.0, 0.0 } });
    const auto filtered = pipeline.createSnapshot();
    CHECK(filtered->solid_surface->GetNumberOfCells() > 0);
    CHECK(filtered->solid_surface->GetNumberOfCells() < original->solid_surface->GetNumberOfCells());
    auto* solid_ids = vtkIdTypeArray::SafeDownCast(filtered->solid_surface->GetCellData()->GetArray("vtkOriginalCellIds"));
    REQUIRE(solid_ids);
    REQUIRE(solid_ids->GetNumberOfValues() == filtered->solid_surface->GetNumberOfCells());
    for (vtkIdType id = 0; id < solid_ids->GetNumberOfValues(); ++id)
        CHECK(solid_ids->GetValue(id) == 0);
    auto* values = filtered->solid_surface->GetCellData()->GetArray("value");
    REQUIRE(values);
    for (vtkIdType id = 0; id < values->GetNumberOfTuples(); ++id)
        CHECK(values->GetComponent(id, 0) == 0.0);
    // 沿用 develop 的完整体内部线框语义，范围筛选仅改变体表面，不重建内部边线缓存。
    CHECK(filtered->solid_internal_edges == original->solid_internal_edges);
    REQUIRE(filtered->solid_internal_edges->GetCellData()->GetArray("value"));
    CHECK(filtered->visible_faces->GetNumberOfCells() == original->visible_faces->GetNumberOfCells());
    CHECK_THROWS_AS(pipeline.setScalarConfig(ScalarDisplayConfig { "value", ElementType::VERTEX, { }, std::array { 0.0, 1.0 } }), std::invalid_argument);
}

TEST_CASE("Scalar range full values survive empty results missing values and clipping", "[scalar-range]")
{
    auto mesh = scalarMesh();
    mesh.face_attributes_["value"][0] = std::numeric_limits<double>::quiet_NaN();
    MeshPipelineData pipeline;
    pipeline.loadModelData(MakeMeshDataVtk(mesh));
    const auto original = pipeline.createSnapshot();
    const auto range = pipeline.scalarRange("value", ElementType::FACE);
    REQUIRE(range);
    CHECK((*range)[0] == 1.0);
    pipeline.setScalarConfig(ScalarDisplayConfig { "value", ElementType::FACE, { }, std::array { 100.0, 200.0 } });
    CHECK(pipeline.createSnapshot()->visible_faces->GetNumberOfCells() == 0);
    vtkNew<vtkPlane> plane;
    plane->SetOrigin(100.0, 0.0, 0.0);
    plane->SetNormal(1.0, 0.0, 0.0);
    pipeline.setClipPlane(plane);
    CHECK(pipeline.scalarRange("value", ElementType::FACE) == range);
    auto config = *pipeline.createSnapshot()->scalar_config;
    config.display.reset();
    pipeline.setScalarConfig(config);
    pipeline.setClipPlane(nullptr);
    CHECK(pipeline.createSnapshot()->visible_faces->GetNumberOfCells() == original->visible_faces->GetNumberOfCells());
    mesh.face_attributes_["constant"].assign(mesh.face_vertices_offset_.size() - 1, 0.5);
    pipeline.loadModelData(MakeMeshDataVtk(mesh));
    const auto constant = pipeline.scalarRange("constant", ElementType::FACE);
    REQUIRE(constant);
    CHECK((*constant)[0] == 0.5);
    CHECK((*constant)[1] == 0.5);
}

TEST_CASE("Scalar config updates report geometry rebuilds and retain the latest completed configuration", "[scalar-range]")
{
    auto mesh = scalarMesh();
    MeshPipelineData pipeline;
    pipeline.loadModelData(MakeMeshDataVtk(mesh));
    const auto original = pipeline.createScalarUpdate();
    REQUIRE(original.geometry_rebuilt);
    ScalarDisplayConfig config { "value", ElementType::FACE, std::array { -1.0, 4.0 }, std::array { 1.0, 3.0 } };
    pipeline.setScalarConfig(config);
    const auto filtered = pipeline.createScalarUpdate();
    REQUIRE(filtered.geometry_rebuilt);

    // 仅修改映射或重复提交相同筛选时，共享已完成的几何，不使选择失效。
    config.mapping = std::array { -2.0, 6.0 };
    pipeline.setScalarConfig(config);
    const auto mapped = pipeline.createScalarUpdate();
    CHECK_FALSE(mapped.geometry_rebuilt);
    CHECK(mapped.snapshot->visible_faces == filtered.snapshot->visible_faces);
    CHECK(mapped.snapshot->visible_edges == filtered.snapshot->visible_edges);
    CHECK(mapped.snapshot->solid_surface == filtered.snapshot->solid_surface);
    REQUIRE(mapped.snapshot->scalar_config);
    CHECK(mapped.snapshot->scalar_config->mapping == config.mapping);
    CHECK(filtered.snapshot->scalar_config->mapping != config.mapping);
    pipeline.setScalarConfig(config);
    const auto repeated = pipeline.createScalarUpdate();
    CHECK_FALSE(repeated.geometry_rebuilt);
    CHECK(repeated.snapshot->visible_faces == mapped.snapshot->visible_faces);

    // 非法请求不改变已接受的配置，也不触发几何重建。
    config.display = std::array { std::numeric_limits<double>::quiet_NaN(), 3.0 };
    CHECK_THROWS_AS(pipeline.setScalarConfig(config), std::invalid_argument);
    const auto rejected = pipeline.createScalarUpdate();
    CHECK_FALSE(rejected.geometry_rebuilt);
    REQUIRE(rejected.snapshot->scalar_config);
    CHECK(rejected.snapshot->scalar_config->display == filtered.snapshot->scalar_config->display);

    // 区间改变会实际重建；即使仍保留原来的三个单元，也不再比较集合来保留选择。
    config.display = std::array { 0.9, 3.1 };
    pipeline.setScalarConfig(config);
    const auto same_cells = pipeline.createScalarUpdate();
    CHECK(same_cells.geometry_rebuilt);
    CHECK(same_cells.snapshot->visible_faces != filtered.snapshot->visible_faces);
    auto* face_ids = vtkIdTypeArray::SafeDownCast(same_cells.snapshot->visible_faces->GetCellData()->GetArray("PrecessFaceIds"));
    REQUIRE(face_ids);
    REQUIRE(face_ids->GetNumberOfValues() == 3);
    for (vtkIdType id = 0; id < face_ids->GetNumberOfValues(); ++id)
        CHECK(face_ids->GetValue(id) == id + 1);

    // 取消全范围筛选仍重建几何，恢复前后单元数相同也应使旧选择失效。
    config.display = pipeline.scalarRange("value", ElementType::FACE);
    pipeline.setScalarConfig(config);
    const auto full_range = pipeline.createScalarUpdate();
    REQUIRE(full_range.geometry_rebuilt);
    CHECK(full_range.snapshot->visible_faces->GetNumberOfCells() == original.snapshot->visible_faces->GetNumberOfCells());
    pipeline.setScalarConfig(std::nullopt);
    const auto canceled = pipeline.createScalarUpdate();
    CHECK(canceled.geometry_rebuilt);
    CHECK(canceled.snapshot->visible_faces->GetNumberOfCells() == full_range.snapshot->visible_faces->GetNumberOfCells());
    CHECK_FALSE(canceled.snapshot->scalar_config);

    // 无筛选的着色与取消只更新完成配置，继续共享几何。
    config.display.reset();
    pipeline.setScalarConfig(config);
    const auto unfiltered = pipeline.createScalarUpdate();
    CHECK_FALSE(unfiltered.geometry_rebuilt);
    REQUIRE(unfiltered.snapshot->scalar_config);
    CHECK(unfiltered.snapshot->visible_faces == canceled.snapshot->visible_faces);
    pipeline.setScalarConfig(std::nullopt);
    const auto unfiltered_canceled = pipeline.createScalarUpdate();
    CHECK_FALSE(unfiltered_canceled.geometry_rebuilt);
    CHECK(unfiltered_canceled.snapshot->visible_faces == unfiltered.snapshot->visible_faces);
    CHECK_FALSE(unfiltered_canceled.snapshot->scalar_config);
    CHECK(pipeline.createSnapshot()->scalar_config == std::nullopt);
}

TEST_CASE("Source reload retains applicable scalar configuration and removes only invalid parts", "[scalar-range]")
{
    auto mesh = scalarMesh();
    MeshPipelineData pipeline;
    pipeline.loadModelData(MakeMeshDataVtk(mesh));
    pipeline.setScalarConfig(ScalarDisplayConfig { "value", ElementType::FACE, { }, std::array { 1.0, 3.0 } });
    const auto before = pipeline.createScalarUpdate();
    pipeline.setVisibility(false);
    pipeline.hideFaces({ 1 });
    pipeline.loadModelData(MakeMeshDataVtk(mesh));
    const auto reloaded = pipeline.createScalarUpdate();
    CHECK(reloaded.geometry_rebuilt);
    CHECK(reloaded.snapshot->generation != before.snapshot->generation);
    REQUIRE(reloaded.snapshot->scalar_config);
    CHECK(reloaded.snapshot->scalar_config->mapping == before.snapshot->scalar_config->mapping);
    CHECK(reloaded.snapshot->visible_faces->GetNumberOfCells() == 3);
    CHECK_FALSE(reloaded.snapshot->visible);
    // 全缺失值只能解除筛选，属性仍可按原有 NaN 规则着色。
    mesh.face_attributes_["value"].assign(mesh.face_attributes_["value"].size(), std::numeric_limits<double>::quiet_NaN());
    pipeline.loadModelData(MakeMeshDataVtk(mesh));
    const auto missing = pipeline.createScalarUpdate();
    REQUIRE(missing.snapshot->scalar_config);
    CHECK_FALSE(missing.snapshot->scalar_config->display);
    CHECK(missing.snapshot->scalar_config->mapping == before.snapshot->scalar_config->mapping);
    // 同名边属性仍在，不能替代已消失的面属性。
    mesh.face_attributes_.erase("value");
    pipeline.loadModelData(MakeMeshDataVtk(mesh));
    CHECK_FALSE(pipeline.createScalarUpdate().snapshot->scalar_config);
}
