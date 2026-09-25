#include "GeometryTopologyDiagnosticActor.h"

#include "CoincidentTopology.h"
#include "GeometryTopologyEditor.h"

#include <spdlog/spdlog.h>

#include <BRepAdaptor_Curve.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <IVtk_Types.hxx>
#include <NCollection_Map.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <vtkCellArray.h>
#include <vtkDataArray.h>
#include <vtkExtractSelection.h>
#include <vtkGeometryFilter.h>
#include <vtkIdTypeArray.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSelection.h>
#include <vtkSelectionNode.h>
#include <gp_Pnt.hxx>

#include <algorithm>
#include <chrono>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <cstdio>
#include <string>
#include <vector>

namespace {
using DiagnosticColor = std::array<double, 3>;

constexpr std::array<DiagnosticColor, kGeometryTopologyDiagnosticCategoryCount> kCategoryColors { {
    { 0.90, 0.10, 0.20 }, // 边界边：红
    { 0.55, 0.25, 0.75 }, // 孤立边：紫/洋红
    { 1.00, 0.75, 0.10 }, // 非流形边：黄
    { 0.00, 0.72, 0.83 }, // 细小边：青
    { 0.10, 0.46, 0.82 }, // 细小面：蓝
    { 0.49, 0.34, 0.76 }, // 重复面：紫
    { 1.00, 0.09, 0.27 }, // 自相交：亮粉红
    { 1.00, 0.54, 0.00 }, // 几何干涉：橙
    { 0.90, 0.10, 0.20 }, // 无效拓扑：红
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

/**
 * @brief 将命中子形状对应的单元一次性提取为独立数据。
 *
 * 诊断结果不再通过过滤器长期连接主几何数据，避免视角变化时反复触发
 * 主几何管线更新。提取后的数据只在诊断结果变化时重建。
 */
void extractDiagnosticCells(vtkPolyData* source,
    vtkDataArray* subshape_ids,
    const NCollection_Map<IVtk_IdType>& selected_subshapes,
    vtkPolyData& output)
{
    output.Initialize();
    if (!source || !subshape_ids || selected_subshapes.Extent() == 0) {
        output.Modified();
        return;
    }

    const vtkIdType cell_count = std::min(
        source->GetNumberOfCells(), subshape_ids->GetNumberOfTuples());
    vtkNew<vtkIdTypeArray> cell_ids;
    cell_ids->SetNumberOfComponents(1);
    for (vtkIdType cell_id = 0; cell_id < cell_count; ++cell_id) {
        const auto subshape_id = static_cast<IVtk_IdType>(subshape_ids->GetTuple1(cell_id));
        if (selected_subshapes.Contains(subshape_id))
            cell_ids->InsertNextValue(cell_id);
    }
    if (cell_ids->GetNumberOfValues() == 0) {
        output.Modified();
        return;
    }

    vtkNew<vtkSelectionNode> selection_node;
    selection_node->SetFieldType(vtkSelectionNode::CELL);
    selection_node->SetContentType(vtkSelectionNode::INDICES);
    selection_node->SetSelectionList(cell_ids);
    vtkNew<vtkSelection> selection;
    selection->AddNode(selection_node);

    vtkNew<vtkExtractSelection> extract_selection;
    extract_selection->SetInputData(0, source);
    extract_selection->SetInputData(1, selection);
    extract_selection->Update();

    vtkNew<vtkGeometryFilter> geometry_filter;
    geometry_filter->SetInputConnection(extract_selection->GetOutputPort());
    geometry_filter->Update();
    output.ShallowCopy(geometry_filter->GetOutput());
    output.Modified();
}
}

GeometryTopologyDiagnosticActor::GeometryTopologyDiagnosticActor(vtkRenderer* renderer)
    : renderer_(renderer)
{
    if (!renderer_)
        throw std::invalid_argument("GeometryTopologyDiagnosticActor: renderer cannot be null");

    auto setup_pipeline = [this](DiagnosticPipeline& pipeline, size_t color_index, bool edge) {
        pipeline.mapper->SetInputData(pipeline.data);
        pipeline.mapper->SetScalarVisibility(false);
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
    interfering_faces_.clear();
    category_computed_.fill(false);

    // 立即清掉旧模型的独立诊断数据，避免新模型计算完成前短暂显示旧结果。
    auto clear_pipeline = [](DiagnosticPipeline& pipeline) {
        pipeline.data->Initialize();
        pipeline.data->Modified();
    };
    for (DiagnosticPipeline& pipeline : pipelines_)
        clear_pipeline(pipeline);
    clear_pipeline(invalid_edge_pipeline_);
    auto clear_marker = [](SizeMarkerPipeline& marker) {
        vtkNew<vtkPoints> points;
        vtkNew<vtkCellArray> vertices;
        marker.data->SetPoints(points);
        marker.data->SetVerts(vertices);
        marker.data->Modified();
    };
    clear_marker(small_edge_marker_);
    clear_marker(small_face_marker_);

    for (size_t index = 0; index < pipelines_.size(); ++index) {
        if (category_enabled_[index])
            rebuildCategory(static_cast<GeometryTopologyDiagnosticCategory>(index));
    }
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

void GeometryTopologyDiagnosticActor::setCategoryFlagsOnly(
    const std::array<bool, kGeometryTopologyDiagnosticCategoryCount>& enabled)
{
    category_enabled_ = enabled;
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
        applyVisibility();
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
        applyVisibility();
    }
}

void GeometryTopologyDiagnosticActor::setInterferingFaces(std::vector<TopoDS_Face> faces)
{
    interfering_faces_ = std::move(faces);
    const size_t category = categoryIndex(GeometryTopologyDiagnosticCategory::InterferingFace);
    category_computed_[category] = true;
    if (category_enabled_[category]) {
        rebuildCategory(GeometryTopologyDiagnosticCategory::InterferingFace);
        applyVisibility();
    }
}

void GeometryTopologyDiagnosticActor::ensureDiagnostics(
    GeometryTopologyDiagnosticCategory category)
{
    const size_t index = categoryIndex(category);
    if (index >= category_computed_.size() || !shape_ || shape_->IsNull())
        return;
    if (category_computed_[index]) {
        // 结果已在缓存里：不重复计算，但仍然输出一次日志——用户重新打开类别时
        // 应该能看到检出内容，而不是被"已算过"静默跳过。
        logCachedDiagnostics(category);
        return;
    }
    if (!diagnostics_)
        diagnostics_ = std::make_unique<GeometryTopologyDiagnosticResult>();

    GeometryTopologyDiagnosticOptions options {
        false, false, false, false, false, false, false
    };
    // Geometry Interference Check 必须同时看到窗口内的全部实体，由 Manager 计算后回填。
    if (category == GeometryTopologyDiagnosticCategory::InterferingFace) {
        category_computed_[index] = true;
        return;
    }
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
    } else if (category == GeometryTopologyDiagnosticCategory::SelfIntersectingFace) {
        // Self Intersections 与重复面共用一次逐对扫描，同时保存重复面结果，保证两类互斥且
        // 后续开关无需重算。
        options.duplicate_faces = true;
        options.self_intersecting_faces = true;
    } else if (category == GeometryTopologyDiagnosticCategory::InvalidTopology) {
        options.invalid_topology = true;
    }

    const auto compute_started = std::chrono::steady_clock::now();
    GeometryTopologyDiagnosticResult computed = GeometryTopologyEditor::diagnoseTopology(
        *shape_, small_edge_length_threshold_, small_face_area_threshold_, options);
    const double elapsed_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - compute_started)
                                  .count();
    logDiagnosticDetails(computed, options, elapsed_ms);
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
    } else if (options.self_intersecting_faces) {
        diagnostics_->duplicate_face_groups = std::move(computed.duplicate_face_groups);
        diagnostics_->self_intersecting_face_pairs
            = std::move(computed.self_intersecting_face_pairs);
        category_computed_[categoryIndex(GeometryTopologyDiagnosticCategory::DuplicateFace)] = true;
        category_computed_[categoryIndex(
            GeometryTopologyDiagnosticCategory::SelfIntersectingFace)]
            = true;
    } else if (options.duplicate_faces) {
        diagnostics_->duplicate_face_groups = std::move(computed.duplicate_face_groups);
        category_computed_[categoryIndex(GeometryTopologyDiagnosticCategory::DuplicateFace)] = true;
    } else if (options.invalid_topology) {
        diagnostics_->invalid_shapes = std::move(computed.invalid_shapes);
        category_computed_[categoryIndex(GeometryTopologyDiagnosticCategory::InvalidTopology)] = true;
    }
}

void GeometryTopologyDiagnosticActor::logCachedDiagnostics(
    GeometryTopologyDiagnosticCategory category)
{
    if (!diagnostics_ || !shape_ || shape_->IsNull())
        return;

    // 几何干涉跨越同一 Model 内的多个 Component，由 QRenderWindow 统一报一行，这里跳过。
    if (category == GeometryTopologyDiagnosticCategory::InterferingFace)
        return;

    // 其余类别映射回各自的选项，复用同一段格式化逻辑（不传耗时）。
    GeometryTopologyDiagnosticOptions options {
        false, false, false, false, false, false, false
    };
    switch (category) {
    case GeometryTopologyDiagnosticCategory::BoundaryEdge:
    case GeometryTopologyDiagnosticCategory::IsolatedEdge:
    case GeometryTopologyDiagnosticCategory::NonManifoldEdge:
        options.edge_topology = true;
        break;
    case GeometryTopologyDiagnosticCategory::SmallEdge:
        options.small_edges = true;
        break;
    case GeometryTopologyDiagnosticCategory::SmallFace:
        options.small_faces = true;
        break;
    case GeometryTopologyDiagnosticCategory::DuplicateFace:
        options.duplicate_faces = true;
        break;
    case GeometryTopologyDiagnosticCategory::SelfIntersectingFace:
        options.duplicate_faces = true;
        options.self_intersecting_faces = true;
        break;
    case GeometryTopologyDiagnosticCategory::InvalidTopology:
        options.invalid_topology = true;
        break;
    default:
        return;
    }
    logDiagnosticDetails(*diagnostics_, options, -1.0);
}

void GeometryTopologyDiagnosticActor::logDiagnosticDetails(
    const GeometryTopologyDiagnosticResult& result,
    const GeometryTopologyDiagnosticOptions& options,
    double elapsed_ms)
{
    // 结果来自缓存时不带耗时（负数约定）；日志里不出现"复用"之类的字样。
    std::string cost;
    if (elapsed_ms >= 0.0) {
        char buffer[32] {};
        std::snprintf(buffer, sizeof(buffer), "耗时 %.1f ms ", elapsed_ms);
        cost = buffer;
    }
    // 每个组件一个诊断 Actor，日志必须标出结果属于哪个组件，否则多组件时无法区分。
    const std::string label = component_label_.empty()
        ? std::string("几何拓扑诊断：")
        : ("几何拓扑诊断（组件 " + component_label_ + "）：");

    // 每个类别只报一行计数：日志回答"检查了什么、查出多少"，
    // 具体是哪些面/边由界面高亮呈现，不写进日志。
    if (options.edge_topology) {
        spdlog::info("{}边界边/孤立边/非流形边 {}—— 边界边 {}，孤立边 {}，非流形边 {}",
            label, cost, result.boundary_edges.size(), result.isolated_edges.size(),
            result.non_manifold_edges.size());
        return;
    }
    if (options.small_edges) {
        spdlog::info("{}细小边 {}—— {} 条（阈值 {:.6g}）", label, cost,
            result.small_edges.size(), small_edge_length_threshold_);
        return;
    }
    if (options.small_faces) {
        spdlog::info("{}细小面 {}—— {} 张（阈值 {:.6g}）", label, cost,
            result.small_faces.size(), small_face_area_threshold_);
        return;
    }
    if (options.duplicate_faces || options.self_intersecting_faces) {
        // 两类共用一次逐对扫描，一起报才不会让调用方误以为漏算。
        spdlog::info("{}重复面/自相交 {}—— 重复面 {} 组，自相交 {} 对", label, cost,
            result.duplicate_face_groups.size(), result.self_intersecting_face_pairs.size());
        return;
    }
    if (options.invalid_topology) {
        spdlog::info("{}无效拓扑 {}—— {} 个", label, cost, result.invalid_shapes.size());
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
    case GeometryTopologyDiagnosticCategory::SelfIntersectingFace:
        for (const GeometryIntersectingFacePair& pair :
            diagnostics_->self_intersecting_face_pairs) {
            appendShapeId(ids, occ_shape_, pair.first);
            appendShapeId(ids, occ_shape_, pair.second);
        }
        break;
    case GeometryTopologyDiagnosticCategory::InterferingFace:
        for (const TopoDS_Face& face : interfering_faces_)
            appendShapeId(ids, occ_shape_, face);
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
        break;
    default:
        break;
    }
    const bool edge_category = isEdgeCategory(category);
    extractDiagnosticCells(edge_category ? line_data_ : face_data_,
        edge_category ? line_sub_ids_ : face_sub_ids_, ids,
        *pipelines_[categoryIndex(category)].data);
    if (category == GeometryTopologyDiagnosticCategory::InvalidTopology) {
        extractDiagnosticCells(line_data_, line_sub_ids_, invalid_edge_ids,
            *invalid_edge_pipeline_.data);
    }
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
    for (size_t index = 0; index < pipelines_.size(); ++index) {
        DiagnosticPipeline& pipeline = pipelines_[index];
        pipeline.actor->SetVisibility(geometry_visible_ && category_enabled_[index]
            && pipeline.data->GetNumberOfCells() > 0);
    }
    invalid_edge_pipeline_.actor->SetVisibility(
        geometry_visible_
        && category_enabled_[categoryIndex(GeometryTopologyDiagnosticCategory::InvalidTopology)]
        && invalid_edge_pipeline_.data->GetNumberOfCells() > 0);
    small_edge_marker_.actor->SetVisibility(
        geometry_visible_
        && category_enabled_[categoryIndex(GeometryTopologyDiagnosticCategory::SmallEdge)]
        && small_edge_marker_.data->GetNumberOfPoints() > 0);
    small_face_marker_.actor->SetVisibility(
        geometry_visible_
        && category_enabled_[categoryIndex(GeometryTopologyDiagnosticCategory::SmallFace)]
        && small_face_marker_.data->GetNumberOfPoints() > 0);
}
