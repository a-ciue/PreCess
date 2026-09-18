#include "GeometryTopologyDiagnosticActor.h"

#include "CoincidentTopology.h"
#include "GeometryTopologyEditor.h"
#include "GeometryTopologyDiagnosticTaskQueue.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <IVtk_Types.hxx>
#include <NCollection_Map.hxx>
#include <Standard_Failure.hxx>
#include <spdlog/spdlog.h>
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
    // 先请后台尽快收手，再丢弃本组件的排队请求与未取走结果。
    if (outstanding_cancel_)
        outstanding_cancel_->store(true);
    GeometryTopologyDiagnosticTaskQueue::shared().discardOwner(this);
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
    invalidateTasks();
    diagnostics_.reset();
    category_computed_.fill(false);

    // 旧结果已作废，必须立刻清空全部过滤集合与标记点：否则在新结果回来之前，各 VTK
    // filter 仍挂着上一份几何的子形状 ID，会在新模型上短暂高亮错误的面和边。
    {
        NCollection_Map<IVtk_IdType> empty_ids;
        for (DiagnosticPipeline& pipeline : pipelines_)
            pipeline.filter->SetData(empty_ids);
        invalid_edge_pipeline_.filter->SetData(empty_ids);
    }
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
    invalidateTasks();
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
    invalidateTasks();
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
        || category_pending_[index] || category_failed_[index]
        || !shape_ || shape_->IsNull()) {
        return;
    }
    // 真正的计算放在后台线程，渲染线程只登记请求后立刻返回，避免长任务卡住视口。
    submitDiagnostics();
}

bool GeometryTopologyDiagnosticActor::pendingOptions(
    GeometryTopologyDiagnosticOptions& options) const
{
    options = GeometryTopologyDiagnosticOptions { false, false, false, false, false, false };
    auto wants = [this](GeometryTopologyDiagnosticCategory category) {
        const size_t index = categoryIndex(category);
        return category_enabled_[index] && !category_computed_[index]
            && !category_pending_[index] && !category_failed_[index];
    };

    bool any = false;
    if (wants(GeometryTopologyDiagnosticCategory::BoundaryEdge)
        || wants(GeometryTopologyDiagnosticCategory::IsolatedEdge)
        || wants(GeometryTopologyDiagnosticCategory::NonManifoldEdge)) {
        options.edge_topology = true;
        any = true;
    }
    if (wants(GeometryTopologyDiagnosticCategory::SmallEdge)) {
        options.small_edges = true;
        any = true;
    }
    if (wants(GeometryTopologyDiagnosticCategory::SmallFace)) {
        options.small_faces = true;
        any = true;
    }
    if (wants(GeometryTopologyDiagnosticCategory::DuplicateFace)) {
        options.duplicate_faces = true;
        any = true;
    }
    if (wants(GeometryTopologyDiagnosticCategory::IntersectingFace)) {
        // 相交检测同时保存重复面结果，保证两类互斥且后续开关无需重算。
        options.duplicate_faces = true;
        options.intersecting_faces = true;
        any = true;
    }
    if (wants(GeometryTopologyDiagnosticCategory::InvalidTopology)) {
        options.invalid_topology = true;
        any = true;
    }
    return any;
}

void GeometryTopologyDiagnosticActor::submitDiagnostics()
{
    // 每个组件同一时刻只允许一个在途请求：形状或阈值变化时直接作废重来即可，
    // 不必让多个请求互相覆盖结果。
    if (outstanding_generation_ != 0 || !shape_ || shape_->IsNull())
        return;

    GeometryTopologyDiagnosticOptions options;
    if (!pendingOptions(options))
        return;

    GeometryTopologyDiagnosticRequest request;
    request.owner = this;
    // 值拷贝一份形状：靠 TShape 引用计数保证后台计算期间底层几何有效。
    request.shape = std::make_shared<const TopoDS_Shape>(*shape_);
    request.small_edge_length_threshold = small_edge_length_threshold_;
    request.small_face_area_threshold = small_face_area_threshold_;
    request.options = options;
    // 取消标记必须在这里保住一份：请求随后被移入队列，移后源的成员已失效。
    std::shared_ptr<std::atomic<bool>> cancel = std::make_shared<std::atomic<bool>>(false);
    request.cancel = cancel;

    outstanding_generation_
        = GeometryTopologyDiagnosticTaskQueue::shared().submit(std::move(request));
    if (outstanding_generation_ == 0)
        return; // 队列已停止，保持"未计算"，不显示误导性的空结果。
    outstanding_cancel_ = std::move(cancel);

    for (size_t index = 0; index < category_pending_.size(); ++index) {
        if (category_enabled_[index] && !category_computed_[index] && !category_failed_[index])
            category_pending_[index] = true;
    }
}

void GeometryTopologyDiagnosticActor::invalidateTasks()
{
    // 协作式取消：置位标记让后台在候选对之间尽快退出，但不强制中断单次 OCC 运算。
    // 即使它没能及时退出，结果也会在回传时被 generation 检查丢掉。
    if (outstanding_cancel_)
        outstanding_cancel_->store(true);
    outstanding_cancel_.reset();
    ++task_generation_;
    outstanding_generation_ = 0;
    category_pending_.fill(false);
    category_failed_.fill(false);
}

void GeometryTopologyDiagnosticActor::markFailedCategories(
    const GeometryTopologyDiagnosticOptions& options)
{
    auto mark = [this](GeometryTopologyDiagnosticCategory category) {
        const size_t index = categoryIndex(category);
        if (category_enabled_[index] && !category_computed_[index])
            category_failed_[index] = true;
    };
    if (options.edge_topology) {
        mark(GeometryTopologyDiagnosticCategory::BoundaryEdge);
        mark(GeometryTopologyDiagnosticCategory::IsolatedEdge);
        mark(GeometryTopologyDiagnosticCategory::NonManifoldEdge);
    }
    if (options.small_edges)
        mark(GeometryTopologyDiagnosticCategory::SmallEdge);
    if (options.small_faces)
        mark(GeometryTopologyDiagnosticCategory::SmallFace);
    if (options.duplicate_faces)
        mark(GeometryTopologyDiagnosticCategory::DuplicateFace);
    if (options.intersecting_faces)
        mark(GeometryTopologyDiagnosticCategory::IntersectingFace);
    if (options.invalid_topology)
        mark(GeometryTopologyDiagnosticCategory::InvalidTopology);
}

void GeometryTopologyDiagnosticActor::mergeOutcome(GeometryTopologyDiagnosticOutcome& outcome)
{
    if (!diagnostics_)
        diagnostics_ = std::make_unique<GeometryTopologyDiagnosticResult>();

    GeometryTopologyDiagnosticResult& computed = outcome.result;
    if (outcome.options.edge_topology) {
        diagnostics_->boundary_edges = std::move(computed.boundary_edges);
        diagnostics_->isolated_edges = std::move(computed.isolated_edges);
        diagnostics_->non_manifold_edges = std::move(computed.non_manifold_edges);
        category_computed_[categoryIndex(GeometryTopologyDiagnosticCategory::BoundaryEdge)] = true;
        category_computed_[categoryIndex(GeometryTopologyDiagnosticCategory::IsolatedEdge)] = true;
        category_computed_[categoryIndex(GeometryTopologyDiagnosticCategory::NonManifoldEdge)] = true;
    }
    if (outcome.options.small_edges) {
        diagnostics_->small_edges = std::move(computed.small_edges);
        category_computed_[categoryIndex(GeometryTopologyDiagnosticCategory::SmallEdge)] = true;
    }
    if (outcome.options.small_faces) {
        diagnostics_->small_faces = std::move(computed.small_faces);
        category_computed_[categoryIndex(GeometryTopologyDiagnosticCategory::SmallFace)] = true;
    }
    if (outcome.options.intersecting_faces) {
        diagnostics_->duplicate_face_groups = std::move(computed.duplicate_face_groups);
        diagnostics_->intersecting_face_pairs = std::move(computed.intersecting_face_pairs);
        category_computed_[categoryIndex(GeometryTopologyDiagnosticCategory::DuplicateFace)] = true;
        category_computed_[categoryIndex(GeometryTopologyDiagnosticCategory::IntersectingFace)] = true;
    } else if (outcome.options.duplicate_faces) {
        diagnostics_->duplicate_face_groups = std::move(computed.duplicate_face_groups);
        category_computed_[categoryIndex(GeometryTopologyDiagnosticCategory::DuplicateFace)] = true;
    }
    if (outcome.options.invalid_topology) {
        diagnostics_->invalid_shapes = std::move(computed.invalid_shapes);
        category_computed_[categoryIndex(GeometryTopologyDiagnosticCategory::InvalidTopology)] = true;
    }
}

bool GeometryTopologyDiagnosticActor::pumpCompletedTasks()
{
    std::vector<GeometryTopologyDiagnosticOutcome> outcomes
        = GeometryTopologyDiagnosticTaskQueue::shared().takeOutcomes(this);
    bool applied = false;
    for (GeometryTopologyDiagnosticOutcome& outcome : outcomes) {
        // 先判世代再动状态：迟到的旧结果不能清掉"更晚那次请求"的在途登记。
        if (outstanding_generation_ == 0 || outcome.generation != outstanding_generation_) {
            // 形状或阈值已经变化，这一份结果作废。把丢弃也记下来，否则"结果一直不
            // 出现"会被误读成"算得很慢"。
            spdlog::info("几何拓扑诊断：丢弃过期结果（代号 {}，当前在途 {}）", outcome.generation,
                outstanding_generation_);
            continue;
        }
        outstanding_generation_ = 0;
        outstanding_cancel_.reset();
        category_pending_.fill(false);
        if (outcome.cancelled) {
            spdlog::info("几何拓扑诊断：本次请求已取消，跳过");
            continue; // 主动取消：不计失败，后续由 submitDiagnostics() 重新排队。
        }
        if (outcome.failed) {
            // 计算失败时保持"未计算"并记录失败，避免立刻重试造成死循环；
            // 只标记本次请求覆盖到的类别，计算期间新启用的类别留给下一次请求。
            // 形状或阈值变化会清掉失败标记，允许重试。
            markFailedCategories(outcome.options);
            continue;
        }
        mergeOutcome(outcome);
        applied = true;
    }
    if (applied) {
        size_t refreshed = 0;
        for (size_t index = 0; index < pipelines_.size(); ++index) {
            if (category_enabled_[index] && category_computed_[index]) {
                rebuildCategory(static_cast<GeometryTopologyDiagnosticCategory>(index));
                ++refreshed;
            }
        }
        // 与"用户勾选类别"的同步路径保持一致：结果就位后再同步一次可见性，
        // 避免只更新了过滤集合而 Actor 仍是隐藏状态。
        applyVisibility();
        spdlog::info("几何拓扑诊断：结果已应用到界面（刷新 {} 个类别）", refreshed);
    }
    // 计算期间又启用了别的类别就继续排队。
    submitDiagnostics();
    return applied;
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
