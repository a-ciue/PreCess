#include "QRenderWindow.h"
#include "renderStrategy/AttributeCommon.h"
#include "FeatureSystem.h"
#include "InteractionService.h"
#include "InteractionState.h"
#include "MeshActorManager.h"
#include "QFeatureSystemAdaptor.h"
#include "QModelQuery.h"
#include "QRenderWindowStyle.h"
#include "QSelection.h"
#include "SelectManager.h"
#include "Selection.h"
#include "GeometryActorManager.h"
#include "GeometryDataVtk.h"
#include "MeshIdQuery.h"

#include <spdlog/spdlog.h>
#include <vtkCamera.h>
#include <vtkDoubleArray.h>
#include <vtkCallbackCommand.h>
#include <vtkDisplaySizedImplicitPlaneRepresentation.h>
#include <vtkDisplaySizedImplicitPlaneWidget.h>
#include <vtkMapper.h>
#include <vtkMath.h>
#include <vtkObjectFactory.h>
#include <vtkPlane.h>

#include <cmath>
#include <utility>

namespace {
//! @brief 相机/视口输入未变则整段跳过；变化时按 1-2-5 整数档反算精确段长并居中重设端点，
//!        使标尺段长与刻度值随相机缩放联动更新
class ScaleBarRangeUpdater : public vtkCommand {
public:
    static ScaleBarRangeUpdater* New() { return new ScaleBarRangeUpdater; }

    vtkAxisActor2D* axis_ {};
    vtkRenderer* renderer_ {};

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
    QModelQuery* query_ {};
};
}

QRenderWindow::QRenderWindow()
{
    connect(this, &QQuickItem::widthChanged, this, &QRenderWindow::resetCamera);
    connect(this, &QQuickItem::heightChanged, this, &QRenderWindow::resetCamera);
}

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
            rep->GetPlane(plane_);
            mesh_actor_manager_->setClipPlane(plane_);
        }
        vtkNew<vtkPlane> plane_;
        MeshActorManager* mesh_actor_manager_ {};
    };
    vtkNew<PlaneCallback> callback;
    callback->mesh_actor_manager_ = vtk->mesh_actor_manager_.get();
    vtk->plane_widget_->SetInteractor(renderWindow->GetInteractor());
    vtk->plane_widget_->SetRepresentation(rep);
    vtk->plane_widget_->AddObserver(vtkCommand::InteractionEvent, callback);

    return vtk;
}

void QRenderWindow::destroyingVTK(vtkRenderWindow* renderWindow, vtkUserData userData)
{
    auto* vtk = Data::SafeDownCast(userData);
    if (!vtk)
        return;

    // 选择与交互服务持有 renderer、manager op 和 pick list，必须在 VTK Data 资源前释放。
    if (vtk->style_) {
        vtk->style_->SetInteractionService(nullptr);
        vtk->style_->SetSelectManager(nullptr);
    }
    interaction_service_.reset();
    select_manager_.reset();

    // 裁剪回调保存 MeshActorManager 裸指针，先停用并移除回调，再销毁 manager。
    if (vtk->plane_widget_) {
        vtk->plane_widget_->Off();
        vtk->plane_widget_->RemoveObservers(vtkCommand::InteractionEvent);
    }
    vtk->geometry_actor_manager_.reset();
    vtk->mesh_actor_manager_.reset();

    if (vtk->renderer_)
        vtk->renderer_->RemoveAllViewProps();
    data_ = nullptr;
}

void QRenderWindow::resetCamera()
{
    dispatch_async([this](vtkRenderWindow* renderWindow, vtkUserData userData) {
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
    dispatch_async([model_id, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);

        auto component_ids = model_query_->getComponentIds(model_id);
        for (Index component_id : component_ids) {
            vtk->mesh_actor_manager_->deleteComponent(component_id);
            vtk->geometry_actor_manager_->deleteComponent(component_id);
        }

        this->select_manager_->clearSelection();
    });
}

void QRenderWindow::deleteComponent(Index component_id)
{
    dispatch_async([component_id, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);

        if (vtk->mesh_actor_manager_) {
            vtk->mesh_actor_manager_->deleteComponent(component_id);
        }

        if (vtk->geometry_actor_manager_) {
            vtk->geometry_actor_manager_->deleteComponent(component_id);
        }

        this->select_manager_->clearSelection();
    });
}

void QRenderWindow::setMeshClip(bool on)
{
    dispatch_async([on, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);

        if (on) {
            double bound[6] {};
            vtk->renderer_->ComputeVisiblePropBounds(bound);

            double origin[3] { (bound[0] + bound[1]) / 2, (bound[2] + bound[3]) / 2, (bound[4] + bound[5]) / 2 };
            vtk->plane_widget_->GetDisplaySizedImplicitPlaneRepresentation()->SetOrigin(origin);

            vtk->plane_widget_->InvokeEvent(vtkCommand::InteractionEvent);
            vtk->plane_widget_->On();
        } else {
            vtk->plane_widget_->Off();
            vtk->mesh_actor_manager_->setClipPlane(nullptr);
        }
    });
}

void QRenderWindow::onModelChanged(Index model_id)
{
    dispatch_async([model_id, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        if (!vtk || !this->model_query_)
            return;

        // Actor 即将重新加载，先释放引用旧 PolyData 和 OCC Shape 的选择器。
        this->select_manager_->clearSelection();
        auto component_ids = model_query_->getComponentIds(model_id);

        for (Index component_id : component_ids) {
            auto mesh_data = model_query_->getMeshDataByComponent(component_id);
            if (mesh_data) {
                vtk->mesh_actor_manager_->loadMesh(component_id, *mesh_data, vtk->renderer_);
            }

            auto geometry_data = model_query_->getGeometryVtkDataByComponent(component_id);
            if (geometry_data) {
                vtk->geometry_actor_manager_->loadGeometry(*geometry_data);
            }
        }
    });
}

void QRenderWindow::onComponentChanged(Index component_id)
{
    dispatch_async([component_id, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);

        if (!vtk || !this->model_query_)
            return;

        // Component 的子形状索引和 Actor 数据会更新，旧高亮选择器不能继续复用。
        this->select_manager_->clearSelection();

        if (vtk->mesh_actor_manager_) {
            auto mesh_data = this->model_query_->getMeshDataByComponent(component_id);
            if (mesh_data) {
                vtk->mesh_actor_manager_->loadMesh(component_id, *mesh_data, vtk->renderer_);
            } else {
                vtk->mesh_actor_manager_->deleteComponent(component_id);
            }
        }

        if (vtk->geometry_actor_manager_) {
            auto geometry_data = this->model_query_->getGeometryVtkDataByComponent(component_id);
            if (geometry_data) {
                vtk->geometry_actor_manager_->loadGeometry(*geometry_data);
            } else {
                vtk->geometry_actor_manager_->deleteComponent(component_id);
            }
        }
    });
}

void QRenderWindow::setVisibility(Index model_id, bool visibility)
{
    dispatch_async([model_id, visibility, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);

        auto component_ids = model_query_->getComponentIds(model_id);
        for (Index component_id : component_ids) {
            vtk->mesh_actor_manager_->setVisibility(component_id, visibility);
            vtk->geometry_actor_manager_->setVisibility(component_id, visibility);
            select_manager_->setMeshHighlightVisible(component_id, visibility);
            select_manager_->setGeometryHighlightVisible(component_id, visibility);
        }
        select_manager_->refreshComponentHighlight();
    });
}

void QRenderWindow::setComponentVisibility(Index component_id, bool visibility)
{
    dispatch_async([component_id, visibility, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);

        if (vtk->mesh_actor_manager_) {
            vtk->mesh_actor_manager_->setVisibility(component_id, visibility);
        }

        if (vtk->geometry_actor_manager_) {
            vtk->geometry_actor_manager_->setVisibility(component_id, visibility);
        }
        select_manager_->setMeshHighlightVisible(component_id, visibility);
        select_manager_->setGeometryHighlightVisible(component_id, visibility);
        select_manager_->refreshComponentHighlight();
    });
}

void QRenderWindow::setMeshVisibility(Index component_id, bool visibility)
{
    dispatch_async([component_id, visibility, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);

        if (vtk->mesh_actor_manager_) {
            vtk->mesh_actor_manager_->setVisibility(component_id, visibility);
        }
        select_manager_->setMeshHighlightVisible(component_id, visibility);
        select_manager_->refreshComponentHighlight();
    });
}

void QRenderWindow::setGeometryVisibility(Index component_id, bool visibility)
{
    dispatch_async([component_id, visibility, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);

        if (vtk->geometry_actor_manager_) {
            vtk->geometry_actor_manager_->setVisibility(component_id, visibility);
        }
        select_manager_->setGeometryHighlightVisible(component_id, visibility);
        select_manager_->refreshComponentHighlight();
    });
}

QSelection* QRenderWindow::selectedIDs()
{
    std::unique_ptr<Selection> data = this->select_manager_->getSelection();
    if (!data) {
        return nullptr;
    }

    // 保留 SelectManager 实际拾取到的 component_id；如果拾取器未提供，再退到当前活动组件
    if (data->component_id < 0) {
        data->component_id = this->cur_component_id_;
    }
    QSelection* selection = new QSelection(std::move(data));
    QJSEngine::setObjectOwnership(selection, QJSEngine::JavaScriptOwnership);
    return selection;
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
    dispatch_async([component_id, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        if (!vtk)
            return;

        this->cur_component_id_ = component_id;
    });
}

void QRenderWindow::setSelectMode(QString select_mode)
{
    dispatch_async([select_mode, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        select_manager_->setSelectMode(select_mode.toStdString());
    });
}

void QRenderWindow::setFaceSelectionByAngle(bool enabled, double angle_deg)
{
    dispatch_async([enabled, angle_deg, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        select_manager_->setFaceSelectionByAngle(enabled, angle_deg);
    });
}

void QRenderWindow::clearSelection()
{
    dispatch_async([this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        this->select_manager_->clearSelection();
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
    dispatch_async([on](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        vtk->scale_bar_axis_->SetVisibility(on);
        });
}

void QRenderWindow::setClick()
{
    dispatch_async([this](vtkRenderWindow* renderWindow, vtkUserData userData) {
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
    dispatch_async([this, component_id, attr_name, mode, args](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
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
    dispatch_async([component_id](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
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
    dispatch_async([style, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
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
    dispatch_async([style, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
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

void QRenderWindow::setTopologyDiagnosticVisible(int category, bool visible)
{
    dispatch_async([category, visible](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        if (vtk->mesh_actor_manager_)
            vtk->mesh_actor_manager_->setTopologyDiagnosticVisible(category, visible);
    });
}

void QRenderWindow::setDihedralAngleRange(double minimum, double maximum)
{
    dispatch_async([minimum, maximum](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        if (vtk->mesh_actor_manager_)
            vtk->mesh_actor_manager_->setDihedralAngleRange(minimum, maximum);
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
