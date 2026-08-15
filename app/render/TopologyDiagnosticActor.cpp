#include "TopologyDiagnosticActor.h"

#include "CoincidentTopology.h"
#include "MeshTopologyDiagnostics.h"

#include <algorithm>
#include <stdexcept>
#include <vtkCellArray.h>
#include <vtkPoints.h>
#include <vtkPlane.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

namespace {
size_t categoryIndex(TopologyDiagnosticCategory category)
{
    return static_cast<size_t>(category);
}

//! @brief 由端点索引集合构造线数据，复用组件局部点坐标
void buildEdgeData(vtkPolyData& output, vtkPoints& points,
    const std::vector<TopologyDiagnosticEdge>& edges)
{
    vtkNew<vtkCellArray> lines;
    for (const TopologyDiagnosticEdge& edge : edges) {
        const vtkIdType ids[2] { edge.endpoints[0], edge.endpoints[1] };
        lines->InsertNextCell(2, ids);
    }
    output.SetPoints(&points);
    output.SetLines(lines);
    output.Modified();
}

//! @brief 由局部点索引集合构造顶点数据
void buildPointData(vtkPolyData& output, vtkPoints& points, const std::vector<Index>& point_ids)
{
    vtkNew<vtkCellArray> vertices;
    for (Index point_id : point_ids) {
        const vtkIdType id = point_id;
        vertices->InsertNextCell(1, &id);
    }
    output.SetPoints(&points);
    output.SetVerts(vertices);
    output.Modified();
}

//! @brief 由面索引集合构造半透明边界面数据
void buildFaceData(vtkPolyData& output, vtkPoints& points,
    const MeshDataVtk& model_data, const std::vector<Index>& face_ids)
{
    vtkNew<vtkCellArray> polygons;
    for (Index face : face_ids) {
        if (face < 0 || face + 1 >= static_cast<Index>(model_data.vtk_face_cells_offset_.size()))
            continue;
        const Index begin = model_data.vtk_face_cells_offset_[face];
        const Index end = model_data.vtk_face_cells_offset_[face + 1];
        if (end - begin < 3)
            continue;
        std::vector<vtkIdType> ids;
        ids.reserve(static_cast<size_t>(end - begin));
        for (Index i = begin; i < end; ++i)
            ids.push_back(model_data.vtk_face_cells_[i]);
        polygons->InsertNextCell(static_cast<vtkIdType>(ids.size()), ids.data());
    }
    output.SetPoints(&points);
    output.SetPolys(polygons);
    output.Modified();
}
}

TopologyDiagnosticActor::TopologyDiagnosticActor(vtkRenderer* renderer)
    : renderer_(renderer)
{
    if (!renderer_)
        throw std::invalid_argument("TopologyDiagnosticActor: renderer cannot be null");

    const std::array<std::array<double, 3>, category_count_> colors { {
        { 0.1, 0.8, 0.2 }, // 边界边：绿色
        { 0.55, 1.0, 0.65 }, // 边界面：淡绿色
        { 1.0, 0.0, 0.0 }, // 非流形边：红色
        { 1.0, 0.0, 0.8 }, // 非流形点：品红色
        { 1.0, 0.5, 0.0 }, // 孤立边：橙色
        { 1.0, 0.5, 0.0 }, // 孤立点：橙色
        { 1.0, 0.9, 0.0 } // 二面角边：黄色
    } };
    for (size_t i = 0; i < category_count_; ++i) {
        mappers_[i]->SetInputData(data_[i]);
        mappers_[i]->SetScalarVisibility(false);
        mappers_[i]->SetRelativeCoincidentTopologyLineOffsetParameters(0, highlight::LINE_UNITS);
        mappers_[i]->SetRelativeCoincidentTopologyPointOffsetParameter(highlight::POINT_UNITS);
        mappers_[i]->SetRelativeCoincidentTopologyPolygonOffsetParameters(0, highlight::POLYGON_UNITS - 1);
        actors_[i]->SetMapper(mappers_[i]);
        actors_[i]->GetProperty()->SetColor(colors[i][0], colors[i][1], colors[i][2]);
        actors_[i]->GetProperty()->SetLineWidth(4.0);
        actors_[i]->GetProperty()->SetPointSize(9.0);
        actors_[i]->PickableOff();
        actors_[i]->SetVisibility(false);
        renderer_->AddActor(actors_[i]);
    }
    actors_[categoryIndex(TopologyDiagnosticCategory::BoundaryFace)]->GetProperty()->SetOpacity(0.35);
}

TopologyDiagnosticActor::~TopologyDiagnosticActor()
{
    if (!renderer_)
        return;
    for (auto& actor : actors_)
        renderer_->RemoveActor(actor);
}

void TopologyDiagnosticActor::loadModelData(const MeshDataVtk& model_data)
{
    diagnostics_ = model_data.topology_diagnostics_;
    points_->Reset();
    for (const auto& position : model_data.vertex_positions_)
        points_->InsertNextPoint(position.data());

    if (!diagnostics_) {
        for (auto& data : data_)
            data->Initialize();
        applyVisibility();
        return;
    }

    buildEdgeData(*data_[categoryIndex(TopologyDiagnosticCategory::BoundaryEdge)], *points_, diagnostics_->boundary_edges);
    buildFaceData(*data_[categoryIndex(TopologyDiagnosticCategory::BoundaryFace)], *points_, model_data, diagnostics_->boundary_faces);
    buildEdgeData(*data_[categoryIndex(TopologyDiagnosticCategory::NonManifoldEdge)], *points_, diagnostics_->non_manifold_edges);
    buildPointData(*data_[categoryIndex(TopologyDiagnosticCategory::NonManifoldVertex)], *points_, diagnostics_->non_manifold_vertices);
    buildEdgeData(*data_[categoryIndex(TopologyDiagnosticCategory::IsolatedEdge)], *points_, diagnostics_->isolated_edges);
    buildPointData(*data_[categoryIndex(TopologyDiagnosticCategory::IsolatedVertex)], *points_, diagnostics_->isolated_vertices);
    rebuildDihedralEdges();
    applyVisibility();
}

void TopologyDiagnosticActor::setCategoryVisible(TopologyDiagnosticCategory category, bool visible)
{
    category_visible_[categoryIndex(category)] = visible;
    applyVisibility();
}

void TopologyDiagnosticActor::setMeshVisible(bool visible)
{
    mesh_visible_ = visible;
    applyVisibility();
}

void TopologyDiagnosticActor::setClipPlane(vtkPlane* plane)
{
    for (size_t i = 0; i < category_count_; ++i) {
        if (plane) {
            // 与主网格面使用同一种提取裁剪管线，保证保留侧的深度和重合拓扑偏移一致。
            clippers_[i]->SetInputData(data_[i]);
            clippers_[i]->SetImplicitFunction(plane);
            mappers_[i]->SetInputConnection(clippers_[i]->GetOutputPort());
        } else {
            mappers_[i]->SetInputData(data_[i]);
        }
    }
}

void TopologyDiagnosticActor::setDihedralAngleRange(double minimum, double maximum)
{
    dihedral_minimum_ = std::clamp(minimum, 0.0, 180.0);
    dihedral_maximum_ = std::clamp(maximum, dihedral_minimum_, 180.0);
    rebuildDihedralEdges();
}

void TopologyDiagnosticActor::rebuildDihedralEdges()
{
    if (!diagnostics_)
        return;

    std::vector<TopologyDiagnosticEdge> filtered;
    for (const auto& edge : diagnostics_->manifold_edges) {
        if (edge.dihedral_angle_degrees >= dihedral_minimum_
            && edge.dihedral_angle_degrees <= dihedral_maximum_) {
            filtered.push_back(edge);
        }
    }
    buildEdgeData(*data_[categoryIndex(TopologyDiagnosticCategory::DihedralEdge)], *points_, filtered);
}

void TopologyDiagnosticActor::applyVisibility()
{
    for (size_t i = 0; i < category_count_; ++i)
        actors_[i]->SetVisibility(mesh_visible_ && category_visible_[i]);
}
