/** @file TestScalarRenderRange.cpp
 * @brief 验证独立范围、完整值域与单元筛选的组合行为。
 */
#include "MakeMeshData.h"
#include "MakeMeshDataVtk.h"
#include "MeshPipelineData.h"
#include "ScalarRenderRange.h"
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

TEST_CASE("Scalar range parameters reset independently and accept outside values", "[scalar-range]")
{
    ScalarRenderRange range({ 0.2, 0.8 });
    REQUIRE(range.setRange(false, -1.0, 2.0));
    REQUIRE(range.setRange(true, 0.3, 0.6));
    range.resetRange(true);
    CHECK(range.mapping == std::array<double, 2> { -1.0, 2.0 });
    CHECK(range.display == range.original);
    CHECK_FALSE(range.filtered);
    REQUIRE(range.setRange(true, 0.4, 0.4));
    range.resetRange(false);
    CHECK(range.mapping == range.original);
    CHECK(range.display == std::array<double, 2> { 0.4, 0.4 });
    CHECK(range.filtered);
    CHECK_FALSE(range.setRange(false, 2.0, 1.0));
    CHECK_FALSE(range.setRange(true, 0.0, std::numeric_limits<double>::infinity()));
    CHECK_FALSE(range.setRange(true, std::numeric_limits<double>::quiet_NaN(), 1.0));
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
    pipeline.setScalarDisplayRange("value", ElementType::EDGE, true, { 1.0, 1.0 });
    const auto edge_filtered = pipeline.createSnapshot();
    CHECK(edge_filtered->visible_faces->GetNumberOfCells() == original->visible_faces->GetNumberOfCells() - 1);
    auto* edge_face_ids = vtkIdTypeArray::SafeDownCast(edge_filtered->visible_faces->GetCellData()->GetArray("PrecessFaceIds"));
    REQUIRE(edge_face_ids);
    CHECK(edge_face_ids->LookupValue(2) == -1);
    pipeline.setScalarDisplayRange("value", ElementType::FACE, true, { 1.0, 3.0 });
    const auto filtered = pipeline.createSnapshot();
    auto* ids = vtkIdTypeArray::SafeDownCast(filtered->visible_faces->GetCellData()->GetArray("PrecessFaceIds"));
    REQUIRE(ids);
    REQUIRE(ids->GetNumberOfValues() == 2);
    CHECK(ids->GetValue(0) == 1);
    CHECK(ids->GetValue(1) == 3);
    CHECK(filtered->visible_edges->GetNumberOfCells() == original->visible_edges->GetNumberOfCells());
    CHECK(filtered->solid_surface->GetNumberOfCells() == original->solid_surface->GetNumberOfCells());
    CHECK(pipeline.scalarRange("value", ElementType::FACE) == domain);
    CHECK(pipeline.clearScalarDisplayRange());
    CHECK_FALSE(pipeline.clearScalarDisplayRange());
    CHECK(pipeline.createSnapshot()->visible_faces->GetNumberOfCells() == original->visible_faces->GetNumberOfCells() - 1);
    CHECK(filtered->visible_faces->GetNumberOfCells() == 2);
}

TEST_CASE("Scalar range filters edge and solid cells but rejects points", "[scalar-range]")
{
    auto mesh = scalarMesh();
    MeshPipelineData pipeline;
    pipeline.loadModelData(MakeMeshDataVtk(mesh));
    const auto original = pipeline.createSnapshot();
    pipeline.setScalarDisplayRange("value", ElementType::EDGE, true, { 1.0, 1.0 });
    const auto edge_filtered = pipeline.createSnapshot();
    REQUIRE(edge_filtered->visible_edges->GetNumberOfCells() == 1);
    auto* edge_ids = vtkIdTypeArray::SafeDownCast(edge_filtered->visible_edges->GetCellData()->GetArray("vtkOriginalCellIds"));
    REQUIRE(edge_ids);
    CHECK(edge_ids->GetValue(0) == 1);
    pipeline.setScalarDisplayRange("value", ElementType::SOLID, true, { 0.0, 0.0 });
    const auto filtered = pipeline.createSnapshot();
    CHECK(filtered->solid_surface->GetNumberOfCells() > 0);
    CHECK(filtered->solid_surface->GetNumberOfCells() < original->solid_surface->GetNumberOfCells());
    REQUIRE(filtered->visible_solid_ids);
    REQUIRE(filtered->visible_solid_ids->GetNumberOfValues() == 1);
    CHECK(filtered->visible_solid_ids->GetValue(0) == 0);
    auto* values = filtered->solid_surface->GetCellData()->GetArray("value");
    REQUIRE(values);
    for (vtkIdType id = 0; id < values->GetNumberOfTuples(); ++id)
        CHECK(values->GetComponent(id, 0) == 0.0);
    // 沿用 develop 的完整体内部线框语义，范围筛选仅改变体表面，不重建内部边线缓存。
    CHECK(filtered->solid_internal_edges == original->solid_internal_edges);
    REQUIRE(filtered->solid_internal_edges->GetCellData()->GetArray("value"));
    CHECK(filtered->visible_faces->GetNumberOfCells() == original->visible_faces->GetNumberOfCells());
    CHECK_THROWS_AS(pipeline.setScalarDisplayRange("value", ElementType::VERTEX, true, { 0.0, 1.0 }), std::invalid_argument);
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
    pipeline.setScalarDisplayRange("value", ElementType::FACE, true, { 100.0, 200.0 });
    CHECK(pipeline.createSnapshot()->visible_faces->GetNumberOfCells() == 0);
    vtkNew<vtkPlane> plane;
    plane->SetOrigin(100.0, 0.0, 0.0);
    plane->SetNormal(1.0, 0.0, 0.0);
    pipeline.setClipPlane(plane);
    CHECK(pipeline.scalarRange("value", ElementType::FACE) == range);
    pipeline.clearScalarDisplayRange();
    pipeline.setClipPlane(nullptr);
    CHECK(pipeline.createSnapshot()->visible_faces->GetNumberOfCells() == original->visible_faces->GetNumberOfCells());
    mesh.face_attributes_["constant"].assign(mesh.face_vertices_offset_.size() - 1, 0.5);
    pipeline.loadModelData(MakeMeshDataVtk(mesh));
    const auto constant = pipeline.scalarRange("constant", ElementType::FACE);
    REQUIRE(constant);
    CHECK((*constant)[0] == 0.5);
    CHECK((*constant)[1] == 0.5);
}
