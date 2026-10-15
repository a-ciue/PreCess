#include "QRenderWindow.h"
#include "FeatureSystem.h"
#include "GeometryActor.h"
#include "GeometryActorManager.h"
#include "GeometryDataVtk.h"
#include "InteractionService.h"
#include "InteractionState.h"
#include "MeshActorManager.h"
#include "MeshIdQuery.h"
#include "QFeatureSystemAdaptor.h"
#include "QModelQuery.h"
#include "QRenderWindowStyle.h"
#include "QSelection.h"
#include "SelectManager.h"
#include "Selection.h"
#include "renderStrategy/AttributeCommon.h"

#include <QScopeGuard>
#include <spdlog/spdlog.h>
#include <vtkCallbackCommand.h>
#include <vtkCamera.h>
#include <vtkDisplaySizedImplicitPlaneRepresentation.h>
#include <vtkDisplaySizedImplicitPlaneWidget.h>
#include <vtkDoubleArray.h>
#include <vtkMapper.h>
#include <vtkMath.h>
#include <vtkObjectFactory.h>
#include <vtkPlane.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {
//! @brief 比例尺刻度字号因子（vtkAxisActor2D::FontFactor，默认 1.0，取值区间 0.1~2.0）
constexpr double kScaleBarFontFactor = 0.7;

//! @brief 相机/视口输入未变则整段跳过；变化时按 1-2-5 整数档反算精确段长并居中重设端点，
//!        使标尺段长与刻度值随相机缩放联动更新
class ScaleBarRangeUpdater : public vtkCommand {
public:
    static ScaleBarRangeUpdater* New() { return new ScaleBarRangeUpdater; }

    vtkAxisActor2D* axis_ { };
    vtkRenderer* renderer_ { };

    void Execute(vtkObject*, unsigned long, void*) override
    {
        vtkCamera* cam = renderer_ ? renderer_->GetActiveCamera() : nullptr;
        if (!axis_ || !cam)
            return;
        const int* size = renderer_->GetSize();
        if (!size || size[0] <= 0 || size[1] <= 0)
            return;

        // 相机/视口输入未变则整段跳过（含 std::tan 与 niceNumber 的超越函数），
        // 空闲时几乎零开销；仅在缩放/调窗口时重算
        const bool parallel = cam->GetParallelProjection() != 0;
        const double parallel_scale = cam->GetParallelScale();
        const double distance = cam->GetDistance();
        const double view_angle = cam->GetViewAngle();
        if (parallel == last_parallel_
            && parallel_scale == last_parallel_scale_
            && distance == last_distance_
            && view_angle == last_view_angle_
            && size[0] == last_size_x_
            && size[1] == last_size_y_)
            return;
        last_parallel_ = parallel;
        last_parallel_scale_ = parallel_scale;
        last_distance_ = distance;
        last_view_angle_ = view_angle;
        last_size_x_ = size[0];
        last_size_y_ = size[1];

        // 解析式求世界/像素比，避免每帧三次投影/反投影矩阵运算：
        // 平行投影窗口世界高度 = 2*parallelScale；透视投影焦平面世界高度 = 2*距离*tan(fov/2)
        double world_per_pixel;
        if (parallel) {
            world_per_pixel = 2.0 * parallel_scale / size[1];
        } else {
            world_per_pixel = 2.0 * distance
                * std::tan(vtkMath::RadiansFromDegrees(view_angle) / 2.0) / size[1];
        }

        // 以视口宽度 22% 为参考段长，换算为参考世界长度后取 1-2-5 整数档
        constexpr double kTargetFrac = 0.22;
        const double ref_world = kTargetFrac * size[0] * world_per_pixel;
        const double nice = niceNumber(ref_world);

        // 按 nice 整数值反算精确像素段长（除数合并为一次乘法，省一次浮点除法），
        // 使标尺始终精确代表 nice 个世界单位；缩放时段长连续伸缩。段长变化才重设端点，
        // 避免无缩放时逐帧重建轴几何
        const double needed_frac = nice / (world_per_pixel * size[0]);
        const double half = needed_frac * 0.5;
        if (needed_frac != last_frac_) {
            last_frac_ = needed_frac;
            axis_->GetPositionCoordinate()->SetValue(0.5 - half, 0.05);
            axis_->GetPosition2Coordinate()->SetValue(0.5 + half, 0.05);
        }

        // 刻度值跨档才 SetRange：vtkAxisActor2D 每次 SetRange 都会重建刻度文本，
        // 同一档位内段长虽变但 nice 值不变，跳过以避免冗余文本重建
        if (nice == last_range_)
            return;
        last_range_ = nice;
        axis_->SetRange(0.0, nice);
    }

private:
    //! @brief 就近量化到 1-2-5 序列（如 0.037→0.05、12.6→10），刻度跨档才变化
    static double niceNumber(double value)
    {
        if (value <= 0.0)
            return 0.0;
        const double exp10 = std::floor(std::log10(value));
        const double base = std::pow(10.0, exp10);
        const double frac = value / base; // 归一化到 [1,10)
        // 1-2-5-10 的几何中值分界，就近取档
        const double nice_frac = frac < 1.5 ? 1.0 : (frac < 3.5 ? 2.0 : (frac < 7.5 ? 5.0 : 10.0));
        return nice_frac * base;
    }

    // 相机/视口输入缓存（早退判定，空闲时跳过 std::tan 与 niceNumber 的超越函数运算）
    bool last_parallel_ = false;
    double last_parallel_scale_ = 0.0;
    double last_distance_ = 0.0;
    double last_view_angle_ = 0.0;
    int last_size_x_ = 0;
    int last_size_y_ = 0;
    // 输出缓存（段长/刻度未变则跳过轴几何与文本重建）
    double last_frac_ = -1.0; //> 上次写入的段长（归一化视口宽度，初始必触发一次）
    double last_range_ = -1.0; //> 上次写入的刻度值（初始必触发一次）
};

//! @brief IMeshIdQuery 到 QModelQuery 的桥接适配器，使 vtkPart 无需感知 Qt/QML 类型
class QModelIdQueryAdapter : public IMeshIdQuery {
public:
    explicit QModelIdQueryAdapter(QModelQuery* query) noexcept
        : query_(query)
    {
    }

    std::optional<Index> findEdgeByEndpoints(Index component_id, Index p0, Index p1) const override
    {
        if (!query_)
            return std::nullopt;
        return query_->findEdgeByEndpoints(component_id, p0, p1);
    }

    Index pointGlobalId(Index component_id, Index local_point_id) const override
    {
        if (!query_)
            return -1;
        return query_->pointGlobalId(component_id, local_point_id);
    }

private:
    QModelQuery* query_ { };
};

}

QRenderWindow::QRenderWindow() = default;

QRenderWindow::~QRenderWindow() = default;

QQuickVTKItem::vtkUserData QRenderWindow::initializeVTK(vtkRenderWindow* renderWindow)
{
    vtkNew<Data> vtk;

    // 渲染窗口统一使用平行投影：高放大倍数下避免透视投影的视锥非线性误差，
    // 测量与比例尺按世界坐标计算天然准确。SetParallelProjection 需在 AddRenderer
    // 之前设置，确保首帧渲染即生效。
    vtk->renderer_->GetActiveCamera()->SetParallelProjection(true);

    // VTK 的 CoincidentTopology 是进程级全局状态，在渲染窗口初始化阶段统一设置。
    vtkMapper::SetResolveCoincidentTopologyToPolygonOffset();
    vtkMapper::SetResolveCoincidentTopologyPolygonOffsetParameters(0.0, 0.0);

    vtk->renderer_->SetBackground(0.5, 0.5, 0.7);
    vtk->renderer_->SetBackground2(0.7, 0.7, 0.7);
    vtk->renderer_->SetGradientBackground(true);

    vtk->style_->SetDefaultRenderer(vtk->renderer_);
    renderWindow->GetInteractor()->SetInteractorStyle(vtk->style_);

    renderWindow->AddRenderer(vtk->renderer_);

    // 叠加渲染层：测量文字标注置顶显示（SetLayer(1) 自动 PreserveColorBuffer，
    // 每帧只清深度、保留颜色，文字不被模型遮挡）
    renderWindow->SetNumberOfLayers(2);
    vtk->overlay_renderer_->SetLayer(1);
    vtk->overlay_renderer_->InteractiveOff();
    vtk->overlay_renderer_->SetActiveCamera(vtk->renderer_->GetActiveCamera()); // 共享相机，免逐帧同步
    renderWindow->AddRenderer(vtk->overlay_renderer_);

    // 比例尺（叠加层底部中央的一段标尺轴）：端点坐标系为归一化视口，初始段长占视口 22%；
    // ScaleBarRangeUpdater 每帧按 1-2-5 整数档反算精确段长并居中重设端点，使标尺始终精确
    // 代表显示的整数世界长度，刻度值与段长随缩放联动更新
    vtk->scale_bar_axis_->GetPositionCoordinate()->SetCoordinateSystemToNormalizedViewport();
    vtk->scale_bar_axis_->GetPositionCoordinate()->SetValue(0.39, 0.05);
    vtk->scale_bar_axis_->GetPosition2Coordinate()->SetCoordinateSystemToNormalizedViewport();
    vtk->scale_bar_axis_->GetPosition2Coordinate()->SetValue(0.61, 0.05);
    // 刻度字号取默认的一半：FontFactor 默认 1.0（取值区间 0.1~2.0），与 LabelFactor 共同决定
    // 标注字号；直接改 TextProperty 的字号会被轴重建刻度时的内部计算覆盖，故走 FontFactor
    vtk->scale_bar_axis_->SetFontFactor(kScaleBarFontFactor);
    vtk->scale_bar_axis_->SetNumberOfLabels(3);
    vtk->scale_bar_axis_->SetLabelFormat("%.6g");
    vtk->scale_bar_axis_->SetTitleVisibility(false);
    vtk->scale_bar_axis_->PickableOff();
    vtk->scale_bar_axis_->SetVisibility(false);
    vtk->overlay_renderer_->AddActor(vtk->scale_bar_axis_);

    vtkNew<ScaleBarRangeUpdater> scale_bar_updater;
    scale_bar_updater->axis_ = vtk->scale_bar_axis_.GetPointer();
    scale_bar_updater->renderer_ = vtk->overlay_renderer_.GetPointer();
    vtk->overlay_renderer_->AddObserver(vtkCommand::StartEvent, scale_bar_updater);

    this->data_ = vtk.GetPointer();
    vtk->mesh_actor_manager_ = std::make_unique<MeshActorManager>();
    vtk->mesh_actor_manager_->bindRender(vtk->renderer_);
    vtk->geometry_actor_manager_ = std::make_unique<GeometryActorManager>();
    vtk->geometry_actor_manager_->bindRender(vtk->renderer_);

    select_manager_ = std::make_unique<SelectManager>(*vtk->renderer_,
        vtk->mesh_actor_manager_->op(), vtk->geometry_actor_manager_->op());
    if (mesh_id_query_)
        select_manager_->setMeshIdQuery(mesh_id_query_.get());
    vtk->style_->SetSelectManager(this->select_manager_.get());

    // 通用交互服务：网格/几何顶点吸附均经选择系统封装接口完成，不接触 picker
    interaction_service_ = std::make_unique<InteractionService>(*vtk->renderer_, *vtk->overlay_renderer_,
        *select_manager_);
    if (mesh_id_query_)
        interaction_service_->setMeshIdQuery(mesh_id_query_.get());
    vtk->style_->SetInteractionService(interaction_service_.get());
    // 交互状态由功能参数开关驱动（FeatureSystem::activeInteraction），渲染层随取随用
    interaction_service_->state_provider = [this]() -> systems::interaction::InteractionState* {
        auto* feature_system = feature_adaptor_ ? feature_adaptor_->featureSystem() : nullptr;
        return feature_system ? feature_system->activeInteraction() : nullptr;
    };
    // 注入渲染刷新回调（与 setFeatureAdaptor 互补，确保两者先后顺序均能生效）
    injectRenderRefreshCallback();

    vtk->orientationWidget->AnimateOff();
    vtk->orientationWidget->SetParentRenderer(vtk->renderer_);
    vtk->orientationWidget->SetInteractor(renderWindow->GetInteractor()); // 设置坐标系控件大小（占屏幕比例）
    vtk->orientationWidget->On();

    vtkNew<vtkDisplaySizedImplicitPlaneRepresentation> rep;
    // 连接回调函数到平面控件，更新裁剪平面
    struct PlaneCallback : public vtkCommand {
        static PlaneCallback* New() { return new PlaneCallback; }
        void Execute(vtkObject* caller, unsigned long, void*) override
        {
            auto plane_widget = reinterpret_cast<vtkDisplaySizedImplicitPlaneWidget*>(caller);
            vtkDisplaySizedImplicitPlaneRepresentation* rep = reinterpret_cast<vtkDisplaySizedImplicitPlaneRepresentation*>(
                plane_widget->GetRepresentation());
            MeshClipPlane parameters;
            parameters.enabled = true;
            rep->GetOrigin(parameters.origin.data());
            rep->GetNormal(parameters.normal.data());
            publish_(parameters);
        }
        std::function<void(const MeshClipPlane&)> publish_;
    };
    vtkNew<PlaneCallback> callback;
    vtk->clip_revision_ = clip_revision_;
    callback->publish_ = [this, scene = vtk.GetPointer()](const MeshClipPlane& plane) {
        publishClipPlane(plane, scene->clip_revision_);
    };
    vtk->plane_widget_->SetInteractor(renderWindow->GetInteractor());
    vtk->plane_widget_->SetRepresentation(rep);
    vtk->plane_widget_->AddObserver(vtkCommand::InteractionEvent, callback);
    if (mesh_clip_.enabled) {
        rep->SetOrigin(mesh_clip_.origin.data());
        rep->SetNormal(mesh_clip_.normal.data());
        vtk->plane_widget_->On();
    }

    // initializeVTK 的 GUI 同步边界只重放已完成结果，不访问 GUI Filter。
    for (const auto& [component_id, snapshot] : mesh_snapshots_)
        vtk->mesh_actor_manager_->loadMesh(component_id, snapshot, vtk->renderer_);
    render_selection_revision_ = selection_revision_;
    vtk->selection_observer_tag_ = vtk->style_->AddObserver(vtkCommand::SelectionChangedEvent, this, &QRenderWindow::publishSelection);
    publishSelection();
    return vtk;
}

void QRenderWindow::destroyingVTK(vtkRenderWindow* renderWindow, vtkUserData userData)
{
    auto* vtk = Data::SafeDownCast(userData);
    // 场景图重建会先释放旧 Data，再调用 initializeVTK；服务持有旧 renderer 的
    // 裸指针，必须在此渲染线程边界销毁，不能拖到下一次 unique_ptr 赋值或 GUI 析构。
    // 回调持有非 VTK 对象 this，必须在服务和场景图拆解前解除。
    vtk->style_->RemoveObserver(vtk->selection_observer_tag_);
    vtk->selection_observer_tag_ = 0;
    vtk->style_->SetInteractionService(nullptr);
    vtk->style_->SetSelectManager(nullptr);
    interaction_service_.reset();
    select_manager_.reset();
    data_ = nullptr;
    if (vtk->renderer_) {
        vtk->renderer_->RemoveAllViewProps();
    }
}

void QRenderWindow::resetCamera()
{
    enqueueRenderOperation([this](vtkRenderWindow* renderWindow, vtkUserData userData) {
        auto* vtk = Data::SafeDownCast(userData);
        if (vtk->renderer_) {
            vtk->renderer_->ResetCamera();
        }
        scheduleRender();
    });
}

bool QRenderWindow::event(QEvent* ev)
{
    switch (ev->type()) {
    case QEvent::MouseButtonPress: {
        auto e = static_cast<QMouseEvent*>(ev);
        _click.reset(e->clone());
        break;
    }
    case QEvent::MouseMove: {
        if (!_click)
            return QQuickVTKItem::event(ev);

        auto e = static_cast<QMouseEvent*>(ev);
        if ((_click->position() - e->position()).manhattanLength() > 5) {
            QQuickVTKItem::event(QScopedPointer<QMouseEvent>(_click.release()).get());
            return QQuickVTKItem::event(e);
        }
        break;
    }
    case QEvent::MouseButtonRelease: {
        if (!_click)
            return QQuickVTKItem::event(ev);

        setClick();
        auto e = static_cast<QMouseEvent*>(ev);
        if (e->button() == Qt::RightButton) {
            emit rightClicked();
            return QQuickVTKItem::event(ev);
        }
        emit clicked();
        return QQuickVTKItem::event(ev);
        break;
    }
    default:
        break;
    }
    return QQuickVTKItem::event(ev);
}

void QRenderWindow::deleteModel(Index model_id)
{
    const auto ids = model_query_ ? model_query_->getComponentIds(model_id) : std::vector<Index> { };
    for (auto id : ids)
        deleteComponent(id);
}

void QRenderWindow::deleteComponent(Index component_id)
{
    enqueueMeshOperation([this, component_id] {
        mesh_pipeline_data_.erase(component_id);
        mesh_snapshots_.erase(component_id);
        geometry_visibility_.erase(component_id);
        dispatch_async([component_id, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
            Data* vtk = Data::SafeDownCast(userData);

            if (vtk->mesh_actor_manager_) {
                vtk->mesh_actor_manager_->deleteComponent(component_id);
            }

            if (vtk->geometry_actor_manager_) {
                vtk->geometry_actor_manager_->deleteComponent(component_id);
            }

            this->select_manager_->clearSelection();
            publishSelection();
        });
    });
}

void QRenderWindow::setMeshClip(bool on)
{
    enqueueMeshOperation([this, on] {
        const auto revision = ++clip_revision_;
        if (!on) {
            auto plane = mesh_clip_;
            plane.enabled = false;
            applyClipPlane(plane);
        }
        dispatch_async([on, revision](vtkRenderWindow*, vtkUserData user_data) {
            auto* vtk = Data::SafeDownCast(user_data);
            vtk->clip_revision_ = revision;
            if (on) {
                double bounds[6] { };
                vtk->renderer_->ComputeVisiblePropBounds(bounds);
                double origin[3] { (bounds[0] + bounds[1]) / 2, (bounds[2] + bounds[3]) / 2,
                    (bounds[4] + bounds[5]) / 2 };
                vtk->plane_widget_->GetDisplaySizedImplicitPlaneRepresentation()->SetOrigin(origin);
                vtk->plane_widget_->InvokeEvent(vtkCommand::InteractionEvent);
                vtk->plane_widget_->On();
            } else {
                vtk->plane_widget_->Off();
            }
        });
    });
}

void QRenderWindow::publishClipPlane(const MeshClipPlane& plane, std::uint64_t revision)
{
    QMetaObject::invokeMethod(&selection_dispatcher_, [this, plane, revision] { enqueueMeshOperation([this, plane, revision] {
                                                                                    if (revision == clip_revision_)
                                                                                        applyClipPlane(plane);
                                                                                }); }, Qt::QueuedConnection);
}

void QRenderWindow::applyClipPlane(const MeshClipPlane& plane)
{
    mesh_clip_ = plane;
    std::vector<std::pair<Index, std::shared_ptr<const MeshDataSnapshot>>> snapshots;
    for (auto& [component_id, pipeline] : mesh_pipeline_data_) {
        pipeline->setClipPlane(plane);
        if (pipeline->generation() != 0)
            snapshots.emplace_back(component_id, prepareMeshSnapshot(component_id));
    }
    dispatch_async([snapshots = std::move(snapshots)](vtkRenderWindow*, vtkUserData user_data) {
        auto* vtk = Data::SafeDownCast(user_data);
        for (const auto& [component_id, snapshot] : snapshots)
            vtk->mesh_actor_manager_->loadMesh(component_id, snapshot, vtk->renderer_);
    });
}

std::shared_ptr<const MeshDataSnapshot> QRenderWindow::prepareMeshSnapshot(Index component_id)
{
    auto snapshot = mesh_pipeline_data_.at(component_id)->createSnapshot();
    mesh_snapshots_[component_id] = snapshot;
    return snapshot;
}

void QRenderWindow::enqueueMeshOperation(std::function<void()> operation)
{
    mesh_operations_.push_back(std::move(operation));
    drainMeshOperations();
}

void QRenderWindow::drainMeshOperations()
{
    if (draining_mesh_operations_)
        return;
    draining_mesh_operations_ = true;
    const auto guard = qScopeGuard([this] { draining_mesh_operations_ = false; });
    while (!face_operation_pending_ && !mesh_operations_.empty()) {
        auto operation = std::move(mesh_operations_.front());
        mesh_operations_.pop_front();
        operation();
    }
}

void QRenderWindow::enqueueRenderOperation(std::function<void(vtkRenderWindow*, vtkUserData)> operation)
{
    enqueueMeshOperation([this, operation = std::move(operation)] { dispatch_async(operation); });
}

void QRenderWindow::loadMeshData(Index component_id, const MeshDataVtk& data)
{
    // 查询结果是借用视图，必须在当前 GUI 调用内捕获，不能把它延迟到命令队列。
    auto pipeline = std::make_shared<MeshPipelineData>();
    pipeline->loadModelData(data);
    enqueueMeshOperation([this, component_id, pipeline] { installMeshData(component_id, pipeline, true); });
}

void QRenderWindow::loadMeshData(Index component_id, std::unique_ptr<MeshPipelineData> pipeline)
{
    std::shared_ptr<MeshPipelineData> prepared = std::move(pipeline);
    enqueueMeshOperation([this, component_id, prepared] { installMeshData(component_id, prepared, false); });
}

void QRenderWindow::installMeshData(Index component_id, std::shared_ptr<MeshPipelineData> pipeline, bool reset_selection)
{
    if (reset_selection) {
        if (const auto previous = mesh_pipeline_data_.find(component_id); previous != mesh_pipeline_data_.end())
            pipeline->setVisibility(previous->second->isVisibilityEnabled());
    }
    pipeline->setClipPlane(mesh_clip_);
    mesh_pipeline_data_[component_id] = std::move(pipeline);
    const auto snapshot = prepareMeshSnapshot(component_id);
    dispatch_async([this, component_id, snapshot, reset_selection](vtkRenderWindow*, vtkUserData user_data) {
        auto* vtk = Data::SafeDownCast(user_data);
        if (reset_selection) {
            select_manager_->clearSelection();
            publishSelection();
        }
        vtk->mesh_actor_manager_->loadMesh(component_id, snapshot, vtk->renderer_);
    });
}

void QRenderWindow::hideSelectedFaces() { requestFaceOperation(FaceOperation::Hide); }
void QRenderWindow::showSelectedFaces() { requestFaceOperation(FaceOperation::Show); }
void QRenderWindow::isolateSelectedFaces() { requestFaceOperation(FaceOperation::Isolate); }
void QRenderWindow::showAllHiddenFaces() { requestFaceOperation(FaceOperation::ShowAll); }

void QRenderWindow::reverseVisibility(const QVariantList& component_ids)
{
    std::vector<Index> ids;
    for (const auto& id : component_ids)
        ids.push_back(id.toInt());
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    if (!ids.empty())
        requestFaceOperation(FaceOperation::Reverse, std::move(ids));
}

void QRenderWindow::requestFaceOperation(FaceOperation operation, std::vector<Index> ids)
{
    enqueueMeshOperation([this, operation, ids = std::move(ids)] {
        // 等选择回传后再放行后续 GUI 命令，避免较早的反转覆盖较晚的显示操作。
        face_operation_pending_ = true;
        dispatch_async([this, operation, ids](vtkRenderWindow*, vtkUserData user_data) {
            auto* vtk = Data::SafeDownCast(user_data);
            SelectedFaces selected;
            MeshGenerations generations;
            const bool uses_selection = operation == FaceOperation::Hide || operation == FaceOperation::Show
                || operation == FaceOperation::Isolate;
            if (uses_selection) {
                selected = select_manager_->selectedFaceIdsByComponent();
                for (const auto& [component_id, faces] : selected) {
                    if (auto actor = vtk->mesh_actor_manager_->getComponentActor(component_id))
                        generations[component_id] = actor->dataGeneration();
                }
            }
            GeometryVisibility geometry;
            for (auto id : vtk->geometry_actor_manager_->op().getAllComponentIds()) {
                if (auto actor = vtk->geometry_actor_manager_->getComponentActor(id))
                    geometry[id] = actor->isVisibilityEnabled();
            }
            if (operation != FaceOperation::Hide && (!uses_selection || !selected.empty())) {
                select_manager_->clearSelection();
                publishSelection();
            }
            // 空选择也回传完成通知，确保后续命令不会永久等待。
            QMetaObject::invokeMethod(&selection_dispatcher_, [this, operation, selected = std::move(selected), generations = std::move(generations), geometry = std::move(geometry), ids] {
                    try {
                        applyFaceOperation(operation, selected, generations, geometry, ids);
                    } catch (const std::exception& error) {
                        spdlog::error("面显隐数据更新失败: {}", error.what());
                    }
                    face_operation_pending_ = false;
                    drainMeshOperations(); }, Qt::QueuedConnection);
        });
    });
}

void QRenderWindow::applyFaceOperation(FaceOperation operation, const SelectedFaces& selected,
    const MeshGenerations& generations, const GeometryVisibility& geometry, const std::vector<Index>& ids)
{
    SelectedFaces valid_faces;
    for (const auto& [id, faces] : selected) {
        const auto pipeline = mesh_pipeline_data_.find(id);
        const auto generation = generations.find(id);
        if (pipeline != mesh_pipeline_data_.end() && generation != generations.end()
            && pipeline->second->generation() == generation->second)
            valid_faces.emplace(id, faces);
    }
    if ((operation == FaceOperation::Hide || operation == FaceOperation::Show || operation == FaceOperation::Isolate)
        && valid_faces.empty())
        return;
    // 已有 GUI 意图优先，连续反转不能重复使用尚未安装到 Actor 的旧开关。
    for (const auto& [id, visible] : geometry)
        geometry_visibility_.try_emplace(id, visible);
    std::vector<std::pair<Index, std::shared_ptr<const MeshDataSnapshot>>> snapshots;
    GeometryVisibility geometry_updates;
    std::vector<std::pair<Index, std::pair<bool, bool>>> notifications;
    auto publish_mesh = [this, &snapshots](Index id) {
        if (mesh_pipeline_data_.at(id)->generation() != 0)
            snapshots.emplace_back(id, prepareMeshSnapshot(id));
    };
    if (operation == FaceOperation::Hide || operation == FaceOperation::Show) {
        for (const auto& [id, faces] : valid_faces) {
            auto& pipeline = *mesh_pipeline_data_.at(id);
            if (operation == FaceOperation::Hide)
                pipeline.hideFaces(faces);
            else {
                pipeline.showFaces(faces);
                const bool geometry_visible = geometry.contains(id) && geometry_visibility_[id];
                notifications.emplace_back(id, std::make_pair(true, geometry_visible));
            }
            publish_mesh(id);
        }
    } else if (operation == FaceOperation::Isolate || operation == FaceOperation::ShowAll) {
        for (auto& [id, pipeline] : mesh_pipeline_data_) {
            if (pipeline->generation() == 0)
                continue;
            if (operation == FaceOperation::ShowAll)
                pipeline->showAllFaces();
            else {
                const auto faces = valid_faces.find(id);
                if (faces != valid_faces.end())
                    pipeline->isolateFaces(faces->second);
                else
                    pipeline->setVisibility(false);
                notifications.emplace_back(id, std::make_pair(pipeline->isVisibilityEnabled(), false));
            }
            publish_mesh(id);
        }
        if (operation == FaceOperation::Isolate) {
            for (const auto& [id, visible] : geometry) {
                geometry_updates[id] = false;
                geometry_visibility_[id] = false;
                if (!mesh_snapshots_.contains(id))
                    notifications.emplace_back(id, std::make_pair(false, false));
            }
        }
    } else {
        for (auto id : ids) {
            const auto found = mesh_pipeline_data_.find(id);
            auto* pipeline = found != mesh_pipeline_data_.end() && found->second->generation() != 0
                ? found->second.get()
                : nullptr;
            const bool mesh_enabled = pipeline && pipeline->isVisibilityEnabled();
            const bool component_visible = mesh_enabled || (geometry.contains(id) && geometry_visibility_[id]);
            bool mesh_visible = false;
            if (pipeline) {
                if (!mesh_enabled && component_visible)
                    pipeline->setVisibility(false);
                else
                    mesh_visible = pipeline->reverseVisibility();
                publish_mesh(id);
            }
            const bool geometry_visible = geometry.contains(id) && !component_visible;
            if (geometry.contains(id)) {
                geometry_updates[id] = geometry_visible;
                geometry_visibility_[id] = geometry_visible;
            }
            notifications.emplace_back(id, std::make_pair(mesh_visible, geometry_visible));
        }
    }
    dispatch_async([this, operation, snapshots = std::move(snapshots),
                       geometry_updates = std::move(geometry_updates), notifications = std::move(notifications)](vtkRenderWindow*, vtkUserData user_data) {
        auto* vtk = Data::SafeDownCast(user_data);
        for (const auto& [id, snapshot] : snapshots) {
            vtk->mesh_actor_manager_->loadMesh(id, snapshot, vtk->renderer_);
            if (operation == FaceOperation::Hide)
                select_manager_->setMeshHighlightVisible(id, false);
        }
        for (const auto& [id, visible] : geometry_updates)
            vtk->geometry_actor_manager_->setVisibility(id, visible);
        for (const auto& [id, visible] : notifications)
            publishComponentVisibility(id, visible.first, visible.second);
        if (operation == FaceOperation::Hide)
            publishSelection();
    });
}

void QRenderWindow::onModelChanged(Index model_id)
{
    if (!model_query_)
        return;
    for (auto component_id : model_query_->getComponentIds(model_id))
        onComponentChanged(component_id);
}

void QRenderWindow::onComponentChanged(Index component_id)
{
    if (!model_query_)
        return;
    if (auto data = model_query_->getMeshDataByComponent(component_id))
        loadMeshData(component_id, *data);
    else {
        enqueueMeshOperation([this, component_id] {
            mesh_pipeline_data_.erase(component_id);
            mesh_snapshots_.erase(component_id);
        });
    }
    enqueueMeshOperation([this, component_id] {
        dispatch_async([component_id, this](vtkRenderWindow*, vtkUserData user_data) {
            auto* vtk = Data::SafeDownCast(user_data);
            if (!model_query_)
                return;
            select_manager_->clearSelection();
            publishSelection();
            // 几何转换仍沿用现有渲染侧路径，本次只迁移主网格 CPU 管线。
            if (auto geometry_data = model_query_->getGeometryVtkDataByComponent(component_id)) {
                vtk->geometry_actor_manager_->loadGeometry(*geometry_data,
                    model_query_->getComponentName(component_id).toStdString());
            } else {
                vtk->geometry_actor_manager_->deleteComponent(component_id);
            }
            if (!model_query_->getMeshDataByComponent(component_id))
                vtk->mesh_actor_manager_->deleteComponent(component_id);
        });
    });
}

void QRenderWindow::setVisibility(Index model_id, bool visibility)
{
    if (!model_query_)
        return;
    for (auto component_id : model_query_->getComponentIds(model_id))
        setComponentVisibility(component_id, visibility);
}

void QRenderWindow::setComponentVisibility(Index component_id, bool visibility)
{
    setMeshVisibility(component_id, visibility);
    setGeometryVisibility(component_id, visibility);
}

void QRenderWindow::setMeshVisibility(Index component_id, bool visibility)
{
    enqueueMeshOperation([this, component_id, visibility] {
        auto& pipeline = mesh_pipeline_data_[component_id];
        if (!pipeline)
            pipeline = std::make_shared<MeshPipelineData>();
        pipeline->setVisibility(visibility);
        // 整体开关不触发 Filter 重建，只更新发布版本和渲染端开关。
        auto snapshot = pipeline->generation() != 0 ? prepareMeshSnapshot(component_id) : nullptr;
        dispatch_async([component_id, visibility, snapshot, this](vtkRenderWindow*, vtkUserData user_data) {
            auto* vtk = Data::SafeDownCast(user_data);
            if (snapshot)
                vtk->mesh_actor_manager_->loadMesh(component_id, snapshot, vtk->renderer_);
            else
                vtk->mesh_actor_manager_->setVisibility(component_id, visibility);
            select_manager_->setMeshHighlightVisible(component_id, visibility);
            select_manager_->refreshComponentHighlight();
        });
    });
}

void QRenderWindow::setGeometryVisibility(Index component_id, bool visibility)
{
    enqueueMeshOperation([this, component_id, visibility] {
        geometry_visibility_[component_id] = visibility;
        dispatch_async([component_id, visibility, this](vtkRenderWindow*, vtkUserData user_data) {
            auto* vtk = Data::SafeDownCast(user_data);
            vtk->geometry_actor_manager_->setVisibility(component_id, visibility);
            select_manager_->setGeometryHighlightVisible(component_id, visibility);
            select_manager_->refreshComponentHighlight();
        });
    });
}

QSelection* QRenderWindow::selectedIDs()
{
    if (!selection_snapshot_)
        return nullptr;
    // QML 包装对象单独持有副本，其生命周期和可变数据不与跨线程快照共享。
    auto* selection = new QSelection(std::make_unique<Selection>(*selection_snapshot_));
    QJSEngine::setObjectOwnership(selection, QJSEngine::JavaScriptOwnership);
    return selection;
}

void QRenderWindow::setSelectionRevision(int revision)
{
    if (selection_revision_ == revision)
        return;
    // GUI 先推进版本，使已经排队、尚未送达的旧参数结果立即失效。
    selection_revision_ = revision;
    selection_snapshot_.reset();
    emit selectionRevisionChanged();
    emit selectedChanged(); // 没有渲染窗口时也立即同步空选择
    // 拾取器及高亮属于渲染线程；与后续发布使用同一版本，避免混入旧选择。
    enqueueRenderOperation([this, revision](vtkRenderWindow*, vtkUserData) {
        render_selection_revision_ = revision;
        select_manager_->clearSelection();
        publishSelection();
    });
}

void QRenderWindow::publishSelection()
{
    auto selection = select_manager_->getSelection();
    if (selection && selection->component_id < 0)
        selection->component_id = cur_component_id_;
    // getSelection 返回独立副本；发布后只读，避免跨线程修改。
    std::shared_ptr<const Selection> snapshot = std::move(selection);
    const int revision = render_selection_revision_;
    QMetaObject::invokeMethod(&selection_dispatcher_, [this, snapshot, revision] {
        // 用户可能在排队期间切换参数或结束选择，旧结果不能覆盖当前参数。
        if (revision != selection_revision_)
            return;
        selection_snapshot_ = snapshot;
        emit selectedChanged(); }, Qt::QueuedConnection);
}

void QRenderWindow::publishComponentVisibility(Index component_id, bool mesh_visible, bool geometry_visible)
{
    // QQuickVTKItem 接管自身事件，通知复用现有 GUI 调度对象。
    QMetaObject::invokeMethod(&selection_dispatcher_, [this, component_id, mesh_visible, geometry_visible] { emit componentVisibilityChanged(component_id, mesh_visible, geometry_visible); }, Qt::QueuedConnection);
}

void QRenderWindow::setModelQuery(QModelQuery* query)
{
    assert(query != nullptr);
    model_query_ = query;
    mesh_id_query_ = std::make_unique<QModelIdQueryAdapter>(query);
    if (select_manager_)
        select_manager_->setMeshIdQuery(mesh_id_query_.get());
    if (interaction_service_)
        interaction_service_->setMeshIdQuery(mesh_id_query_.get());
}

void QRenderWindow::setSelectComponent(Index component_id)
{
    enqueueRenderOperation([component_id, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        if (!vtk)
            return;

        this->cur_component_id_ = component_id;
    });
}

void QRenderWindow::setSelectMode(QString select_mode)
{
    enqueueRenderOperation([select_mode, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        select_manager_->setSelectMode(select_mode.toStdString());
        publishSelection();
    });
}

void QRenderWindow::setFaceSelectionByAngle(bool enabled, double angle_deg)
{
    enqueueRenderOperation([enabled, angle_deg, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        select_manager_->setFaceSelectionByAngle(enabled, angle_deg);
    });
}

void QRenderWindow::clearSelection()
{
    enqueueRenderOperation([this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        this->select_manager_->clearSelection();
        publishSelection();
    });
}

void QRenderWindow::setFeatureAdaptor(QObject* adaptor)
{
    feature_adaptor_ = qobject_cast<systems::feature::QFeatureSystemAdaptor*>(adaptor);
    // 注入渲染刷新回调：功能经 requestRefresh() 触发 VTK 重绘
    injectRenderRefreshCallback();
}

void QRenderWindow::setScaleBarVisible(bool on)
{
    enqueueRenderOperation([on](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        vtk->scale_bar_axis_->SetVisibility(on);
    });
}

void QRenderWindow::setClick()
{
    enqueueRenderOperation([this](vtkRenderWindow* renderWindow, vtkUserData userData) {
        Data* vtk = Data::SafeDownCast(userData);
        vtk->style_->SetClick();
    });
}

void QRenderWindow::setAttriMode(
    Index component_id,
    QString attr_name,
    int mode,
    QVariantMap args)
{
    if (component_id < 0)
        return;
    enqueueRenderOperation([this, component_id, attr_name, mode, args](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        Mode mode_enum = static_cast<Mode>(mode);

        // QVariantMap 转 std::map<std::string, std::any>
        std::map<std::string, std::any> std_args;
        for (auto it = args.constBegin(); it != args.constEnd(); ++it) {
            if (it.value().type() == QVariant::Double) {
                std_args[it.key().toStdString()] = it.value().toDouble();
            } else if (it.value().type() == QVariant::Int) {
                std_args[it.key().toStdString()] = it.value().toInt();
            } else if (it.value().type() == QVariant::String) {
                std_args[it.key().toStdString()] = it.value().toString().toStdString();
            } else if (it.value().type() == QVariant::List) {
                QVariantList list = it.value().toList();
                std::vector<double> vec;
                for (const auto& v : list) {
                    if (v.canConvert<double>())
                        vec.push_back(v.toDouble());
                }
                std_args[it.key().toStdString()] = vec;
            } else {
                spdlog::error("Unsupported QVariant type,type:{}", QString(QMetaType::typeName(it.value().type())).toStdString());
            }
        }
        spdlog::info("modeEnum: {}", static_cast<int>(mode_enum));
        if (vtk->mesh_actor_manager_ && vtk->mesh_actor_manager_->getCount(component_id)) {
            vtk->mesh_actor_manager_->setAttriMode(
                component_id,
                attr_name.toStdString(),
                mode_enum,
                std_args);
        }
        spdlog::info("-----setAttriMode:" + attr_name.toStdString());
    });
}

void QRenderWindow::cancelComponentAttri(Index component_id)
{
    if (component_id < 0)
        return;
    enqueueRenderOperation([component_id](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        if (vtk->mesh_actor_manager_ && vtk->mesh_actor_manager_->getCount(component_id)) {
            vtk->mesh_actor_manager_->cancelAttri(
                component_id);
        }
        spdlog::info("--------cancelAttri-----------");
    });
}

void QRenderWindow::setGeometryStyle(int style)
{
    geometry_style_ = static_cast<GeometryRenderStyle>(style);
    enqueueRenderOperation([style, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        if (vtk->geometry_actor_manager_) {
            vtk->geometry_actor_manager_->setCurrentRenderStyle(static_cast<GeometryRenderStyle>(style));
        }
        if (select_manager_) {
            select_manager_->setGeometryHighlightVisible(style != 7);
            select_manager_->refreshComponentHighlight();
        }
    });
    emit geometryStyleChanged();
}

int QRenderWindow::getGeometryStyle()
{
    return static_cast<int>(geometry_style_);
}

void QRenderWindow::setMeshStyle(int style)
{
    mesh_style_ = static_cast<MeshRenderStyle>(style);
    enqueueRenderOperation([style, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        if (vtk->mesh_actor_manager_) {
            vtk->mesh_actor_manager_->setCurrentRenderStyle(static_cast<MeshRenderStyle>(style));
        }
        if (select_manager_) {
            select_manager_->setMeshHighlightVisible(style != 7);
            select_manager_->refreshComponentHighlight();
        }
    });
    emit meshStyleChanged();
}

int QRenderWindow::getMeshStyle()
{
    return static_cast<int>(mesh_style_);
}

void QRenderWindow::setTopologyDiagnosticCategoryEnabled(int category, bool enabled)
{
    enqueueRenderOperation([category, enabled](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        if (vtk->mesh_actor_manager_)
            vtk->mesh_actor_manager_->setTopologyDiagnosticCategoryEnabled(category, enabled);
    });
}

void QRenderWindow::setDihedralAngleRange(double minimum, double maximum)
{
    enqueueRenderOperation([minimum, maximum](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        if (vtk->mesh_actor_manager_)
            vtk->mesh_actor_manager_->setDihedralAngleRange(minimum, maximum);
    });
}

void QRenderWindow::setGeometryTopologyDiagnosticCategoryEnabled(int category, bool enabled)
{
    enqueueRenderOperation([category, enabled, this](vtkRenderWindow* renderWindow,
                               vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        if (!vtk || !vtk->geometry_actor_manager_)
            return;
        vtk->geometry_actor_manager_->setTopologyDiagnosticCategoryEnabled(category, enabled);
    });
}

void QRenderWindow::setGeometryTopologyDiagnosticSmallEdgeLength(double threshold)
{
    enqueueRenderOperation([threshold](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        if (!vtk || !vtk->geometry_actor_manager_)
            return;
        vtk->geometry_actor_manager_->setTopologyDiagnosticSmallEdgeLength(threshold);
    });
}

void QRenderWindow::setGeometryTopologyDiagnosticSmallFaceArea(double threshold)
{
    enqueueRenderOperation([threshold](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        if (!vtk || !vtk->geometry_actor_manager_)
            return;
        vtk->geometry_actor_manager_->setTopologyDiagnosticSmallFaceArea(threshold);
    });
}

vtkStandardNewMacro(QRenderWindow::Data);

void QRenderWindow::injectRenderRefreshCallback()
{
    if (!feature_adaptor_ || !feature_adaptor_->featureSystem() || !interaction_service_)
        return;
    auto refresh = [this]() {
        dispatch_async([this](vtkRenderWindow* rw, vtkUserData) {
            interaction_service_->syncPending(); // 同步激活迁移并拉取标注到 VTK actor
            rw->Render();
        });
    };
    feature_adaptor_->featureSystem()->setRenderRefreshCallback(refresh);
    // 注入即排空一次：requestRefresh 的合并跳过以"置位必有在途消费者"为前提，
    // 兜底注入前（回调缺失期）置位而未被消费的挂起刷新
    refresh();
}
