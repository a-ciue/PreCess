#include "TopologyDiagnosticActor.h"

#include "CoincidentTopology.h"
#include "MeshTopologyDiagnostics.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkPoints.h>
#include <vtkPlane.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkUnsignedCharArray.h>

namespace {
using DiagnosticColor = std::array<unsigned char, 3>;

constexpr DiagnosticColor kBoundaryEdgeColor { 26, 204, 51 };
constexpr DiagnosticColor kBoundaryFaceColor { 140, 255, 166 };
constexpr DiagnosticColor kNonManifoldEdgeColor { 255, 0, 0 };
constexpr DiagnosticColor kNonManifoldVertexColor { 255, 0, 204 };
constexpr DiagnosticColor kIsolatedColor { 255, 128, 0 };
constexpr DiagnosticColor kDihedralEdgeColor { 255, 230, 0 };

// 诊断层位于普通网格和选择高亮之间，避免诊断颜色遮挡选中结果。
constexpr double kDiagnosticPointUnits = highlight::POINT_UNITS + 1.0;
constexpr double kDiagnosticLineUnits = highlight::LINE_UNITS + 1.0;
constexpr double kDiagnosticPolygonUnits = highlight::POLYGON_UNITS + 0.5;

size_t categoryIndex(TopologyDiagnosticCategory category)
{
    return static_cast<size_t>(category);
}

//! @brief 把一条诊断边追加到共享线单元，并写入对应颜色
void appendEdge(vtkCellArray& lines, vtkUnsignedCharArray& colors,
    const TopologyDiagnosticEdge& edge, const DiagnosticColor& color)
{
    const vtkIdType ids[2] { edge.endpoints[0], edge.endpoints[1] };
    lines.InsertNextCell(2, ids);
    colors.InsertNextTypedTuple(color.data());
}

//! @brief 把同一类别的诊断边追加到共享线单元，并写入对应颜色
void appendEdges(vtkCellArray& lines, vtkUnsignedCharArray& colors,
    const std::vector<TopologyDiagnosticEdge>& edges, const DiagnosticColor& color)
{
    for (const TopologyDiagnosticEdge& edge : edges)
        appendEdge(lines, colors, edge, color);
}

//! @brief 把同一类别的诊断点追加到共享顶点单元，并写入对应颜色
void appendPoints(vtkCellArray& vertices, vtkUnsignedCharArray& colors,
    const std::vector<Index>& point_ids, const DiagnosticColor& color)
{
    for (Index point_id : point_ids) {
        const vtkIdType id = point_id;
        vertices.InsertNextCell(1, &id);
        colors.InsertNextTypedTuple(color.data());
    }
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
    output.GetCellData()->SetScalars(nullptr);
    output.Modified();
}

//! @brief 配置点或边管线使用 CellData 中的直接 RGB 颜色
void setupColorPipeline(vtkPolyDataMapper& mapper, vtkPolyData& data)
{
    mapper.SetInputData(&data);
    mapper.SetScalarModeToUseCellData();
    mapper.SetColorModeToDirectScalars();
    mapper.SetScalarVisibility(true);
}
}

TopologyDiagnosticActor::TopologyDiagnosticActor(vtkRenderer* renderer)
    : renderer_(renderer)
{
    if (!renderer_)
        throw std::invalid_argument("TopologyDiagnosticActor: renderer cannot be null");

    setupColorPipeline(*point_pipeline_.mapper, *point_pipeline_.data);
    point_pipeline_.mapper->SetRelativeCoincidentTopologyPointOffsetParameter(kDiagnosticPointUnits);
    point_pipeline_.actor->SetMapper(point_pipeline_.mapper);
    point_pipeline_.actor->GetProperty()->SetPointSize(9.0);

    setupColorPipeline(*edge_pipeline_.mapper, *edge_pipeline_.data);
    edge_pipeline_.mapper->SetRelativeCoincidentTopologyLineOffsetParameters(0, kDiagnosticLineUnits);
    edge_pipeline_.actor->SetMapper(edge_pipeline_.mapper);
    edge_pipeline_.actor->GetProperty()->SetLineWidth(4.0);

    face_pipeline_.mapper->SetInputData(face_pipeline_.data);
    face_pipeline_.mapper->SetScalarVisibility(false);
    face_pipeline_.mapper->SetRelativeCoincidentTopologyPolygonOffsetParameters(0, kDiagnosticPolygonUnits);
    face_pipeline_.actor->SetMapper(face_pipeline_.mapper);
    face_pipeline_.actor->GetProperty()->SetColor(
        kBoundaryFaceColor[0] / 255.0, kBoundaryFaceColor[1] / 255.0, kBoundaryFaceColor[2] / 255.0);
    face_pipeline_.actor->GetProperty()->SetOpacity(0.35);

    for (DiagnosticPipeline* pipeline : { &point_pipeline_, &edge_pipeline_, &face_pipeline_ }) {
        pipeline->actor->PickableOff();
        pipeline->actor->SetVisibility(false);
        renderer_->AddActor(pipeline->actor);
    }
}

TopologyDiagnosticActor::~TopologyDiagnosticActor()
{
    if (!renderer_)
        return;
    renderer_->RemoveActor(point_pipeline_.actor);
    renderer_->RemoveActor(edge_pipeline_.actor);
    renderer_->RemoveActor(face_pipeline_.actor);
}

void TopologyDiagnosticActor::loadModelData(const MeshDataVtk& model_data)
{
    diagnostics_ = model_data.topology_diagnostics_;
    points_->Reset();
    for (const auto& position : model_data.vertex_positions_)
        points_->InsertNextPoint(position.data());

    if (!diagnostics_) {
        point_pipeline_.data->Initialize();
        edge_pipeline_.data->Initialize();
        face_pipeline_.data->Initialize();
        applyVisibility();
        return;
    }

    buildFaceData(*face_pipeline_.data, *points_, model_data, diagnostics_->boundary_faces);
    rebuildPointData();
    rebuildEdgeData();
    applyVisibility();
}

void TopologyDiagnosticActor::setCategoryEnabled(TopologyDiagnosticCategory category, bool enabled)
{
    const size_t index = categoryIndex(category);
    if (category_enabled_[index] == enabled)
        return;

    category_enabled_[index] = enabled;
    switch (category) {
    case TopologyDiagnosticCategory::NonManifoldVertex:
    case TopologyDiagnosticCategory::IsolatedVertex:
        rebuildPointData();
        break;
    case TopologyDiagnosticCategory::BoundaryEdge:
    case TopologyDiagnosticCategory::NonManifoldEdge:
    case TopologyDiagnosticCategory::IsolatedEdge:
    case TopologyDiagnosticCategory::DihedralEdge:
        rebuildEdgeData();
        break;
    default:
        break;
    }
    applyVisibility();
}

void TopologyDiagnosticActor::setMeshVisible(bool visible)
{
    mesh_visible_ = visible;
    applyVisibility();
}

void TopologyDiagnosticActor::setClipPlane(vtkPlane* plane)
{
    for (DiagnosticPipeline* pipeline : { &point_pipeline_, &edge_pipeline_, &face_pipeline_ }) {
        if (plane) {
            // 与主网格面使用同一种提取裁剪管线，保证保留侧的深度和重合拓扑偏移一致。
            pipeline->clipper->SetInputData(pipeline->data);
            pipeline->clipper->SetImplicitFunction(plane);
            pipeline->mapper->SetInputConnection(pipeline->clipper->GetOutputPort());
        } else {
            pipeline->mapper->SetInputData(pipeline->data);
        }
    }
}

void TopologyDiagnosticActor::setDihedralAngleRange(double minimum, double maximum)
{
    const double clamped_minimum = std::clamp(minimum, 0.0, 180.0);
    const double clamped_maximum = std::clamp(maximum, clamped_minimum, 180.0);
    if (dihedral_minimum_ == clamped_minimum && dihedral_maximum_ == clamped_maximum)
        return;

    dihedral_minimum_ = clamped_minimum;
    dihedral_maximum_ = clamped_maximum;
    rebuildEdgeData();
}

void TopologyDiagnosticActor::rebuildPointData()
{
    if (!diagnostics_)
        return;

    vtkNew<vtkCellArray> vertices;
    vtkNew<vtkUnsignedCharArray> colors;
    colors->SetName("TopologyDiagnosticColors");
    colors->SetNumberOfComponents(3);

    if (category_enabled_[categoryIndex(TopologyDiagnosticCategory::NonManifoldVertex)]) {
        appendPoints(*vertices, *colors, diagnostics_->non_manifold_vertices, kNonManifoldVertexColor);
    }
    if (category_enabled_[categoryIndex(TopologyDiagnosticCategory::IsolatedVertex)])
        appendPoints(*vertices, *colors, diagnostics_->isolated_vertices, kIsolatedColor);

    point_pipeline_.data->SetPoints(points_);
    point_pipeline_.data->SetVerts(vertices);
    point_pipeline_.data->GetCellData()->SetScalars(colors);
    point_pipeline_.data->Modified();
}

void TopologyDiagnosticActor::rebuildEdgeData()
{
    if (!diagnostics_)
        return;

    vtkNew<vtkCellArray> lines;
    vtkNew<vtkUnsignedCharArray> colors;
    colors->SetName("TopologyDiagnosticColors");
    colors->SetNumberOfComponents(3);

    if (category_enabled_[categoryIndex(TopologyDiagnosticCategory::BoundaryEdge)])
        appendEdges(*lines, *colors, diagnostics_->boundary_edges, kBoundaryEdgeColor);
    if (category_enabled_[categoryIndex(TopologyDiagnosticCategory::NonManifoldEdge)])
        appendEdges(*lines, *colors, diagnostics_->non_manifold_edges, kNonManifoldEdgeColor);
    if (category_enabled_[categoryIndex(TopologyDiagnosticCategory::IsolatedEdge)])
        appendEdges(*lines, *colors, diagnostics_->isolated_edges, kIsolatedColor);
    if (category_enabled_[categoryIndex(TopologyDiagnosticCategory::DihedralEdge)]) {
        for (const TopologyDiagnosticEdge& edge : diagnostics_->manifold_edges) {
            if (edge.dihedral_angle_degrees >= dihedral_minimum_
                && edge.dihedral_angle_degrees <= dihedral_maximum_) {
                appendEdge(*lines, *colors, edge, kDihedralEdgeColor);
            }
        }
    }

    edge_pipeline_.data->SetPoints(points_);
    edge_pipeline_.data->SetLines(lines);
    edge_pipeline_.data->GetCellData()->SetScalars(colors);
    edge_pipeline_.data->Modified();
}

void TopologyDiagnosticActor::applyVisibility()
{
    const bool point_enabled
        = category_enabled_[categoryIndex(TopologyDiagnosticCategory::NonManifoldVertex)]
        || category_enabled_[categoryIndex(TopologyDiagnosticCategory::IsolatedVertex)];
    const bool edge_enabled
        = category_enabled_[categoryIndex(TopologyDiagnosticCategory::BoundaryEdge)]
        || category_enabled_[categoryIndex(TopologyDiagnosticCategory::NonManifoldEdge)]
        || category_enabled_[categoryIndex(TopologyDiagnosticCategory::IsolatedEdge)]
        || category_enabled_[categoryIndex(TopologyDiagnosticCategory::DihedralEdge)];
    const bool face_enabled = category_enabled_[categoryIndex(TopologyDiagnosticCategory::BoundaryFace)];

    point_pipeline_.actor->SetVisibility(mesh_visible_ && point_enabled);
    edge_pipeline_.actor->SetVisibility(mesh_visible_ && edge_enabled);
    face_pipeline_.actor->SetVisibility(mesh_visible_ && face_enabled);
}
