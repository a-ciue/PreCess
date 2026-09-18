#include "GeometryTopologyDiagnosticActor.h"

#include "CoincidentTopology.h"
#include "GeometryTopologyEditor.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <IVtk_Types.hxx>
#include <NCollection_Map.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <vtkDataArray.h>
#include <vtkCellArray.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <gp_Pnt.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using DiagnosticColor = std::array<double, 3>;

constexpr std::array<DiagnosticColor, kGeometryTopologyDiagnosticCategoryCount> kCategoryColors { {
    { 0.10, 0.80, 0.20 }, // 边界边
    { 1.00, 0.50, 0.00 }, // 孤立边
    { 1.00, 0.00, 0.00 }, // 非流形边
    { 1.00, 0.35, 0.70 }, // 细小边
    { 1.00, 0.00, 0.80 }, // 细小面
    { 0.00, 0.80, 1.00 }, // 重复面
    { 1.00, 0.90, 0.00 }, // 相交面
    { 0.55, 0.15, 1.00 }, // 无效拓扑
} };

constexpr double kDiagnosticLineUnits = highlight::LINE_UNITS + 1.0;
constexpr double kDiagnosticPolygonUnits = highlight::POLYGON_UNITS + 0.5;
constexpr double kDiagnosticPointUnits = highlight::POINT_UNITS + 1.0;

size_t categoryIndex(GeometryTopologyDiagnosticCategory category)
{
    return static_cast<size_t>(category);
}

bool isEdgeCategory(GeometryTopologyDiagnosticCategory category)
{
    return category == GeometryTopologyDiagnosticCategory::BoundaryEdge
        || category == GeometryTopologyDiagnosticCategory::IsolatedEdge
        || category == GeometryTopologyDiagnosticCategory::NonManifoldEdge
        || category == GeometryTopologyDiagnosticCategory::SmallEdge;
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

    auto setup_marker = [this](SizeMarkerPipeline& marker, size_t color_index, float point_size) {
        marker.mapper->SetInputData(marker.data);
        marker.mapper->SetScalarVisibility(false);
        marker.mapper->SetRelativeCoincidentTopologyPointOffsetParameter(kDiagnosticPointUnits);
        marker.actor->SetMapper(marker.mapper);
        const DiagnosticColor& color = kCategoryColors[color_index];
        marker.actor->GetProperty()->SetColor(color[0], color[1], color[2]);
        marker.actor->GetProperty()->SetPointSize(point_size);
        marker.actor->GetProperty()->RenderPointsAsSpheresOn();
        marker.actor->GetProperty()->LightingOff();
        marker.actor->PickableOff();
        marker.actor->SetVisibility(false);
        renderer_->AddActor(marker.actor);
    };
    setup_marker(small_edge_marker_,
        categoryIndex(GeometryTopologyDiagnosticCategory::SmallEdge), 10.0F);
    setup_marker(small_face_marker_,
        categoryIndex(GeometryTopologyDiagnosticCategory::SmallFace), 12.0F);
}

GeometryTopologyDiagnosticActor::~GeometryTopologyDiagnosticActor()
{
    if (!renderer_)
        return;
    for (DiagnosticPipeline& pipeline : pipelines_)
        renderer_->RemoveActor(pipeline.actor);
    renderer_->RemoveActor(invalid_edge_pipeline_.actor);
    renderer_->RemoveActor(small_edge_marker_.actor);
    renderer_->RemoveActor(small_face_marker_.actor);
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
    category_computed_.fill(false);

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
    if (enabled)
        rebuildCategory(category);
    applyVisibility();
}

void GeometryTopologyDiagnosticActor::setGeometryVisible(bool visible)
{
    geometry_visible_ = visible;
    applyVisibility();
}

void GeometryTopologyDiagnosticActor::setSmallEdgeLengthThreshold(double threshold)
{
    if (!std::isfinite(threshold) || threshold <= 0.0
        || small_edge_length_threshold_ == threshold) {
        return;
    }
    small_edge_length_threshold_ = threshold;
    const size_t category = categoryIndex(GeometryTopologyDiagnosticCategory::SmallEdge);
    category_computed_[category] = false;
    if (diagnostics_)
        diagnostics_->small_edges.clear();
    if (category_enabled_[category]) {
        rebuildCategory(GeometryTopologyDiagnosticCategory::SmallEdge);
    }
}

void GeometryTopologyDiagnosticActor::setSmallFaceAreaThreshold(double threshold)
{
    if (!std::isfinite(threshold) || threshold <= 0.0
        || small_face_area_threshold_ == threshold) {
        return;
    }
    small_face_area_threshold_ = threshold;
    const size_t category = categoryIndex(GeometryTopologyDiagnosticCategory::SmallFace);
    category_computed_[category] = false;
    if (diagnostics_)
        diagnostics_->small_faces.clear();
    if (category_enabled_[category]) {
        rebuildCategory(GeometryTopologyDiagnosticCategory::SmallFace);
    }
}

void GeometryTopologyDiagnosticActor::ensureDiagnostics(
    GeometryTopologyDiagnosticCategory category)
{
    const size_t index = categoryIndex(category);
    if (index >= category_computed_.size() || category_computed_[index]
        || !shape_ || shape_->IsNull()) {
        return;
    }
    if (!diagnostics_)
        diagnostics_ = std::make_unique<GeometryTopologyDiagnosticResult>();

    GeometryTopologyDiagnosticOptions options {
        false, false, false, false, false, false
    };
    if (category == GeometryTopologyDiagnosticCategory::BoundaryEdge
        || category == GeometryTopologyDiagnosticCategory::IsolatedEdge
        || category == GeometryTopologyDiagnosticCategory::NonManifoldEdge) {
        options.edge_topology = true;
    } else if (category == GeometryTopologyDiagnosticCategory::SmallEdge) {
        options.small_edges = true;
    } else if (category == GeometryTopologyDiagnosticCategory::SmallFace) {
        options.small_faces = true;
    } else if (category == GeometryTopologyDiagnosticCategory::DuplicateFace) {
        options.duplicate_faces = true;
        options.intersecting_faces
            = category_enabled_[categoryIndex(GeometryTopologyDiagnosticCategory::IntersectingFace)];
    } else if (category == GeometryTopologyDiagnosticCategory::IntersectingFace) {
        // 相交检测同时保存重复面结果，保证两类互斥且后续开关无需重算。
        options.duplicate_faces = true;
        options.intersecting_faces = true;
    } else if (category == GeometryTopologyDiagnosticCategory::InvalidTopology) {
        options.invalid_topology = true;
    }

    GeometryTopologyDiagnosticResult computed = GeometryTopologyEditor::diagnoseTopology(
        *shape_, small_edge_length_threshold_, small_face_area_threshold_, options);
    if (options.edge_topology) {
        diagnostics_->boundary_edges = std::move(computed.boundary_edges);
        diagnostics_->isolated_edges = std::move(computed.isolated_edges);
        diagnostics_->non_manifold_edges = std::move(computed.non_manifold_edges);
        category_computed_[categoryIndex(GeometryTopologyDiagnosticCategory::BoundaryEdge)] = true;
        category_computed_[categoryIndex(GeometryTopologyDiagnosticCategory::IsolatedEdge)] = true;
        category_computed_[categoryIndex(GeometryTopologyDiagnosticCategory::NonManifoldEdge)] = true;
    } else if (options.small_edges) {
        diagnostics_->small_edges = std::move(computed.small_edges);
        category_computed_[categoryIndex(GeometryTopologyDiagnosticCategory::SmallEdge)] = true;
    } else if (options.small_faces) {
        diagnostics_->small_faces = std::move(computed.small_faces);
        category_computed_[categoryIndex(GeometryTopologyDiagnosticCategory::SmallFace)] = true;
    } else if (options.intersecting_faces) {
        diagnostics_->duplicate_face_groups = std::move(computed.duplicate_face_groups);
        diagnostics_->intersecting_face_pairs = std::move(computed.intersecting_face_pairs);
        category_computed_[categoryIndex(GeometryTopologyDiagnosticCategory::DuplicateFace)] = true;
        category_computed_[categoryIndex(GeometryTopologyDiagnosticCategory::IntersectingFace)] = true;
    } else if (options.duplicate_faces) {
        diagnostics_->duplicate_face_groups = std::move(computed.duplicate_face_groups);
        category_computed_[categoryIndex(GeometryTopologyDiagnosticCategory::DuplicateFace)] = true;
    } else if (options.invalid_topology) {
        diagnostics_->invalid_shapes = std::move(computed.invalid_shapes);
        category_computed_[categoryIndex(GeometryTopologyDiagnosticCategory::InvalidTopology)] = true;
    }
}

void GeometryTopologyDiagnosticActor::rebuildCategory(GeometryTopologyDiagnosticCategory category)
{
    ensureDiagnostics(category);
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
    case GeometryTopologyDiagnosticCategory::SmallEdge:
        for (const TopoDS_Edge& edge : diagnostics_->small_edges)
            appendShapeId(ids, occ_shape_, edge);
        break;
    case GeometryTopologyDiagnosticCategory::SmallFace:
        for (const TopoDS_Face& face : diagnostics_->small_faces)
            appendShapeId(ids, occ_shape_, face);
        break;
    case GeometryTopologyDiagnosticCategory::DuplicateFace:
        for (const GeometryDuplicateFaceGroup& group : diagnostics_->duplicate_face_groups) {
            for (const TopoDS_Face& face : group.faces)
                appendShapeId(ids, occ_shape_, face);
        }
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
    rebuildSizeMarker(category);
}

void GeometryTopologyDiagnosticActor::rebuildSizeMarker(
    GeometryTopologyDiagnosticCategory category)
{
    if (category != GeometryTopologyDiagnosticCategory::SmallEdge
        && category != GeometryTopologyDiagnosticCategory::SmallFace) {
        return;
    }

    SizeMarkerPipeline& marker = category == GeometryTopologyDiagnosticCategory::SmallEdge
        ? small_edge_marker_
        : small_face_marker_;
    vtkNew<vtkPoints> points;
    vtkNew<vtkCellArray> vertices;
    auto append_point = [&points, &vertices](const gp_Pnt& point) {
        const vtkIdType point_id = points->InsertNextPoint(point.X(), point.Y(), point.Z());
        vertices->InsertNextCell(1, &point_id);
    };

    if (category == GeometryTopologyDiagnosticCategory::SmallEdge) {
        for (const TopoDS_Edge& edge : diagnostics_->small_edges) {
            try {
                BRepAdaptor_Curve curve(edge);
                const double parameter = (curve.FirstParameter() + curve.LastParameter()) * 0.5;
                append_point(curve.Value(parameter));
            } catch (const Standard_Failure&) {
                // 无法计算中点的边仍保留原始着色，不额外生成标记。
            }
        }
    } else {
        for (const TopoDS_Face& face : diagnostics_->small_faces) {
            try {
                GProp_GProps properties;
                BRepGProp::SurfaceProperties(face, properties);
                append_point(properties.CentreOfMass());
            } catch (const Standard_Failure&) {
                // 无法计算质心的面仍保留原始着色，不额外生成标记。
            }
        }
    }

    marker.data->SetPoints(points);
    marker.data->SetVerts(vertices);
    marker.data->Modified();
}

void GeometryTopologyDiagnosticActor::applyVisibility()
{
    for (size_t index = 0; index < pipelines_.size(); ++index)
        pipelines_[index].actor->SetVisibility(geometry_visible_ && category_enabled_[index]);
    invalid_edge_pipeline_.actor->SetVisibility(
        geometry_visible_ && category_enabled_[categoryIndex(GeometryTopologyDiagnosticCategory::InvalidTopology)]);
    small_edge_marker_.actor->SetVisibility(
        geometry_visible_ && category_enabled_[categoryIndex(GeometryTopologyDiagnosticCategory::SmallEdge)]);
    small_face_marker_.actor->SetVisibility(
        geometry_visible_ && category_enabled_[categoryIndex(GeometryTopologyDiagnosticCategory::SmallFace)]);
}
