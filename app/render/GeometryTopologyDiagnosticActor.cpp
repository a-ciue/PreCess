#include "GeometryTopologyDiagnosticActor.h"

#include "CoincidentTopology.h"
#include "GeometryTopologyEditor.h"

#include <IVtk_Types.hxx>
#include <NCollection_Map.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <vtkDataArray.h>
#include <vtkPolyData.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace {
using DiagnosticColor = std::array<double, 3>;

constexpr std::array<DiagnosticColor, kGeometryTopologyDiagnosticCategoryCount> kCategoryColors { {
    { 0.10, 0.80, 0.20 }, // 边界边
    { 1.00, 0.50, 0.00 }, // 孤立边
    { 1.00, 0.00, 0.00 }, // 非流形边
    { 0.00, 0.80, 1.00 }, // 重复面
    { 1.00, 0.00, 0.80 }, // 退化面
    { 1.00, 0.90, 0.00 }, // 相交面
    { 0.55, 0.15, 1.00 }, // 无效拓扑
} };

constexpr double kDiagnosticLineUnits = highlight::LINE_UNITS + 1.0;
constexpr double kDiagnosticPolygonUnits = highlight::POLYGON_UNITS + 0.5;

size_t categoryIndex(GeometryTopologyDiagnosticCategory category)
{
    return static_cast<size_t>(category);
}

bool isEdgeCategory(GeometryTopologyDiagnosticCategory category)
{
    return category == GeometryTopologyDiagnosticCategory::BoundaryEdge
        || category == GeometryTopologyDiagnosticCategory::IsolatedEdge
        || category == GeometryTopologyDiagnosticCategory::NonManifoldEdge;
}

void appendShapeId(NCollection_Map<IVtk_IdType>& ids,
    const GeometryDiagnosticOccShapeHandle& occ_shape,
    const TopoDS_Shape& shape)
{
    if (occ_shape.IsNull() || shape.IsNull())
        return;
    const IVtk_IdType id = occ_shape->GetSubShapeId(shape);
    if (id >= 0)
        ids.Add(id);
}
}

GeometryTopologyDiagnosticActor::GeometryTopologyDiagnosticActor(vtkRenderer* renderer)
    : renderer_(renderer)
{
    if (!renderer_)
        throw std::invalid_argument("GeometryTopologyDiagnosticActor: renderer cannot be null");

    auto setup_pipeline = [this](DiagnosticPipeline& pipeline, size_t color_index, bool edge) {
        pipeline.filter->SetDoFiltering(true);
        pipeline.mapper->SetInputConnection(pipeline.filter->GetOutputPort());
        if (edge)
            pipeline.mapper->SetRelativeCoincidentTopologyLineOffsetParameters(0, kDiagnosticLineUnits);
        else
            pipeline.mapper->SetRelativeCoincidentTopologyPolygonOffsetParameters(0, kDiagnosticPolygonUnits);
        pipeline.actor->SetMapper(pipeline.mapper);
        const DiagnosticColor& color = kCategoryColors[color_index];
        pipeline.actor->GetProperty()->SetColor(color[0], color[1], color[2]);
        pipeline.actor->GetProperty()->LightingOff();
        pipeline.actor->GetProperty()->SetLineWidth(edge ? 4.0 : 1.0);
        pipeline.actor->GetProperty()->SetOpacity(edge ? 1.0 : 0.40);
        pipeline.actor->PickableOff();
        pipeline.actor->SetVisibility(false);
        renderer_->AddActor(pipeline.actor);
    };

    for (size_t index = 0; index < pipelines_.size(); ++index) {
        setup_pipeline(pipelines_[index], index,
            isEdgeCategory(static_cast<GeometryTopologyDiagnosticCategory>(index)));
    }
    setup_pipeline(invalid_edge_pipeline_,
        categoryIndex(GeometryTopologyDiagnosticCategory::InvalidTopology), true);
}

GeometryTopologyDiagnosticActor::~GeometryTopologyDiagnosticActor()
{
    if (!renderer_)
        return;
    for (DiagnosticPipeline& pipeline : pipelines_)
        renderer_->RemoveActor(pipeline.actor);
    renderer_->RemoveActor(invalid_edge_pipeline_.actor);
}

void GeometryTopologyDiagnosticActor::loadShape(const TopoDS_Shape& shape,
    const GeometryDiagnosticOccShapeHandle& occ_shape,
    vtkPolyData* line_data,
    vtkDataArray* line_sub_ids,
    vtkPolyData* face_data,
    vtkDataArray* face_sub_ids)
{
    owned_shape_ = std::make_unique<TopoDS_Shape>(shape);
    shape_ = owned_shape_.get();
    occ_shape_ = occ_shape;
    line_data_ = line_data;
    face_data_ = face_data;
    line_sub_ids_ = line_sub_ids;
    face_sub_ids_ = face_sub_ids;
    diagnostics_.reset();

    for (size_t index = 0; index < pipelines_.size(); ++index) {
        const bool edge = isEdgeCategory(static_cast<GeometryTopologyDiagnosticCategory>(index));
        DiagnosticPipeline& pipeline = pipelines_[index];
        pipeline.filter->SetInputData(edge ? line_data_ : face_data_);
        vtkDataArray* ids = edge ? line_sub_ids_ : face_sub_ids_;
        if (ids && ids->GetName())
            pipeline.filter->SetIdsArrayName(ids->GetName());
        if (category_enabled_[index])
            rebuildCategory(static_cast<GeometryTopologyDiagnosticCategory>(index));
    }
    invalid_edge_pipeline_.filter->SetInputData(line_data_);
    if (line_sub_ids_ && line_sub_ids_->GetName())
        invalid_edge_pipeline_.filter->SetIdsArrayName(line_sub_ids_->GetName());
    applyVisibility();
}

void GeometryTopologyDiagnosticActor::setCategoryEnabled(
    GeometryTopologyDiagnosticCategory category, bool enabled)
{
    const size_t index = categoryIndex(category);
    if (index >= category_enabled_.size() || category_enabled_[index] == enabled)
        return;
    category_enabled_[index] = enabled;
    if (enabled) {
        diagnostics_.reset();
        rebuildCategory(category);
    }
    applyVisibility();
}

void GeometryTopologyDiagnosticActor::setGeometryVisible(bool visible)
{
    geometry_visible_ = visible;
    applyVisibility();
}

void GeometryTopologyDiagnosticActor::setCleanupTolerance(double tolerance)
{
    if (!std::isfinite(tolerance) || tolerance <= 0.0 || cleanup_tolerance_ == tolerance)
        return;
    cleanup_tolerance_ = tolerance;
    diagnostics_.reset();
    for (size_t index = 0; index < category_enabled_.size(); ++index) {
        if (category_enabled_[index])
            rebuildCategory(static_cast<GeometryTopologyDiagnosticCategory>(index));
    }
}

void GeometryTopologyDiagnosticActor::ensureDiagnostics()
{
    if (!diagnostics_ && shape_ && !shape_->IsNull()) {
        GeometryTopologyDiagnosticOptions options;
        options.edge_topology
            = category_enabled_[categoryIndex(GeometryTopologyDiagnosticCategory::BoundaryEdge)]
            || category_enabled_[categoryIndex(GeometryTopologyDiagnosticCategory::IsolatedEdge)]
            || category_enabled_[categoryIndex(GeometryTopologyDiagnosticCategory::NonManifoldEdge)];
        options.duplicate_faces
            = category_enabled_[categoryIndex(GeometryTopologyDiagnosticCategory::DuplicateFace)];
        options.degenerated_faces
            = category_enabled_[categoryIndex(GeometryTopologyDiagnosticCategory::DegeneratedFace)];
        options.intersecting_faces
            = category_enabled_[categoryIndex(GeometryTopologyDiagnosticCategory::IntersectingFace)];
        options.invalid_topology
            = category_enabled_[categoryIndex(GeometryTopologyDiagnosticCategory::InvalidTopology)];
        diagnostics_ = std::make_unique<GeometryTopologyDiagnosticResult>(
            GeometryTopologyEditor::diagnoseTopology(*shape_, cleanup_tolerance_, options));
    }
}

void GeometryTopologyDiagnosticActor::rebuildCategory(GeometryTopologyDiagnosticCategory category)
{
    ensureDiagnostics();
    if (!diagnostics_)
        return;

    NCollection_Map<IVtk_IdType> ids;
    NCollection_Map<IVtk_IdType> invalid_edge_ids;
    switch (category) {
    case GeometryTopologyDiagnosticCategory::BoundaryEdge:
        for (const TopoDS_Edge& edge : diagnostics_->boundary_edges)
            appendShapeId(ids, occ_shape_, edge);
        break;
    case GeometryTopologyDiagnosticCategory::IsolatedEdge:
        for (const TopoDS_Edge& edge : diagnostics_->isolated_edges)
            appendShapeId(ids, occ_shape_, edge);
        break;
    case GeometryTopologyDiagnosticCategory::NonManifoldEdge:
        for (const TopoDS_Edge& edge : diagnostics_->non_manifold_edges)
            appendShapeId(ids, occ_shape_, edge);
        break;
    case GeometryTopologyDiagnosticCategory::DuplicateFace:
        for (const GeometryDuplicateFaceGroup& group : diagnostics_->duplicate_face_groups) {
            for (const TopoDS_Face& face : group.faces)
                appendShapeId(ids, occ_shape_, face);
        }
        break;
    case GeometryTopologyDiagnosticCategory::DegeneratedFace:
        for (const TopoDS_Face& face : diagnostics_->degenerated_faces)
            appendShapeId(ids, occ_shape_, face);
        break;
    case GeometryTopologyDiagnosticCategory::IntersectingFace:
        for (const GeometryIntersectingFacePair& pair : diagnostics_->intersecting_face_pairs) {
            appendShapeId(ids, occ_shape_, pair.first);
            appendShapeId(ids, occ_shape_, pair.second);
        }
        break;
    case GeometryTopologyDiagnosticCategory::InvalidTopology:
        for (const TopoDS_Shape& shape : diagnostics_->invalid_shapes) {
            if (shape.ShapeType() == TopAbs_FACE)
                appendShapeId(ids, occ_shape_, shape);
            else if (shape.ShapeType() == TopAbs_EDGE || shape.ShapeType() == TopAbs_VERTEX)
                appendShapeId(invalid_edge_ids, occ_shape_, shape);
            else if (shape.ShapeType() == TopAbs_WIRE) {
                for (TopExp_Explorer edge(shape, TopAbs_EDGE); edge.More(); edge.Next())
                    appendShapeId(invalid_edge_ids, occ_shape_, edge.Current());
            }
        }
        invalid_edge_pipeline_.filter->SetData(invalid_edge_ids);
        break;
    default:
        break;
    }
    pipelines_[categoryIndex(category)].filter->SetData(ids);
}

void GeometryTopologyDiagnosticActor::applyVisibility()
{
    for (size_t index = 0; index < pipelines_.size(); ++index)
        pipelines_[index].actor->SetVisibility(geometry_visible_ && category_enabled_[index]);
    invalid_edge_pipeline_.actor->SetVisibility(
        geometry_visible_ && category_enabled_[categoryIndex(GeometryTopologyDiagnosticCategory::InvalidTopology)]);
}
