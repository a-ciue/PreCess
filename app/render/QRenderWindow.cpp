#include "QRenderWindow.h"
#include "renderStrategy/AttributeCommon.h"
#include "MeshActorManager.h"
#include "QModelQuery.h"
#include "QRenderWindowStyle.h"
#include "QSelection.h"
#include "SelectManager.h"
#include "Selection.h"
#include "GeometryActorManager.h"
#include "GeometryDataVtk.h"

#include <spdlog/spdlog.h>
#include <vtkDoubleArray.h>
#include <vtkCallbackCommand.h>
#include <vtkDisplaySizedImplicitPlaneRepresentation.h>
#include <vtkDisplaySizedImplicitPlaneWidget.h>
#include <vtkMapper.h>
#include <vtkMath.h>
#include <vtkObjectFactory.h>
#include <vtkPlane.h>

#include <cmath>

namespace {
//! @brief 每帧渲染前把比例尺段的像素长度换算为世界长度写入 Range，使刻度值随相机缩放更新
//! （与 vtkLegendScaleActor::UpdateAxisRange 同法：按相机焦点深度反投影轴两端点）
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
        if (!size || size[1] <= 0)
            return;

        // 段像素长度（轴两端点为归一化视口坐标）
        const double* v1 = axis_->GetPositionCoordinate()->GetValue();
        const double* v2 = axis_->GetPosition2Coordinate()->GetValue();
        const double pixel_len = std::abs(v2[0] - v1[0]) * size[0];

        // 解析式求世界长度，避免每帧三次投影/反投影矩阵运算：
        // 平行投影窗口世界高度 = 2*parallelScale；透视投影焦平面世界高度 = 2*距离*tan(fov/2)
        double world_per_pixel;
        if (cam->GetParallelProjection()) {
            world_per_pixel = 2.0 * cam->GetParallelScale() / size[1];
        } else {
            world_per_pixel = 2.0 * cam->GetDistance()
                * std::tan(vtkMath::RadiansFromDegrees(cam->GetViewAngle()) / 2.0) / size[1];
        }
        const double nice = niceNumber(pixel_len * world_per_pixel);

        // 刻度值跨档才 SetRange：vtkAxisActor2D 每次 SetRange 都会重建刻度文本，
        // 连续缩放时逐帧重建是主要帧率开销
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

    double last_range_ = -1.0; //> 上次写入的刻度值（初始必触发一次）
};
}

QRenderWindow::QRenderWindow()
{
    connect(this, &QQuickItem::widthChanged, this, &QRenderWindow::resetCamera);
    connect(this, &QQuickItem::heightChanged, this, &QRenderWindow::resetCamera);
    edge_render_ = false;
}

QRenderWindow::~QRenderWindow() = default;

QQuickVTKItem::vtkUserData QRenderWindow::initializeVTK(vtkRenderWindow* renderWindow)
{
    vtkNew<Data> vtk;

    // Note:  It is okay to store some non-graphical VTK objects in the QQuickVTKItem instead of the
    // vtkUserData but ONLY if they are accessed from the qml-render-thread. (i.e. only in the
    // initializeVTK, destroyingVTK or dispatch_async methods)
    // vtk->renderer->GetActiveCamera()->DeepCopy(_camera);

    // VTK 的 CoincidentTopology 是进程级全局状态，在渲染窗口初始化阶段统一设置。
    vtkMapper::SetResolveCoincidentTopologyToPolygonOffset();
    vtkMapper::SetResolveCoincidentTopologyPolygonOffsetParameters(0.0, 0.0);

    vtk->renderer_->SetBackground(0.5, 0.5, 0.7);
    vtk->renderer_->SetBackground2(0.7, 0.7, 0.7);
    vtk->renderer_->SetGradientBackground(true);

    vtk->style_->SetDefaultRenderer(vtk->renderer_);
    renderWindow->GetInteractor()->SetInteractorStyle(vtk->style_);

    renderWindow->AddRenderer(vtk->renderer_);

    // 叠加渲染层：标尺/标注置顶显示（SetLayer(1) 自动 PreserveColorBuffer，
    // 每帧只清深度、保留颜色，不被模型遮挡）
    renderWindow->SetNumberOfLayers(2);
    vtk->overlay_renderer_->SetLayer(1);
    vtk->overlay_renderer_->InteractiveOff();
    vtk->overlay_renderer_->SetActiveCamera(vtk->renderer_->GetActiveCamera()); // 共享相机，免逐帧同步
    renderWindow->AddRenderer(vtk->overlay_renderer_);

    // 比例尺（叠加层底部中央的一段标尺轴）：端点视口位置固定，ScaleBarRangeUpdater
    // 每帧把段长换算为世界长度写入 Range，刻度值随缩放自动更新
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
    vtk->mesh_actor_manager_ = std::make_unique<MeshActorManager>(vtk->global_points_.GetPointer());
    vtk->mesh_actor_manager_->bindRender(vtk->renderer_);
    vtk->geometry_actor_manager_ = std::make_unique<GeometryActorManager>();
    vtk->geometry_actor_manager_->bindRender(vtk->renderer_);

    select_manager_ = std::make_unique<SelectManager>(*vtk->renderer_,
        vtk->mesh_actor_manager_->op(), vtk->geometry_actor_manager_->op());
    vtk->style_->SetSelectManager(this->select_manager_.get());

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
    if (vtk->renderer_) {
        _camera->DeepCopy(vtk->renderer_->GetActiveCamera());
        vtk->renderer_->RemoveAllViewProps();
    }
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

void QRenderWindow::updateGlobalVtkPointsImpl(Data* vtk)
{
    if (!vtk || !model_query_)
        return;

    const auto& pts = model_query_->globalPoints();
    auto count = static_cast<vtkIdType>(pts.size());
    vtkIdType totalVals = count * 3;

    vtkNew<vtkDoubleArray> arr;
    arr->SetNumberOfComponents(3);
    // 纯几何模型没有全局网格点，不能对空 vector 的 data() 继续解引用。
    if (!pts.empty())
        arr->SetArray(const_cast<double*>(pts.front().data()), totalVals, 1);

    vtk->global_points_->SetData(arr);

    if (vtk->mesh_actor_manager_) {
        vtk->mesh_actor_manager_->syncOriginalPointIds();
    }

    spdlog::info("[VTK GlobalPoints] updated: N={}", (int)vtk->global_points_->GetNumberOfPoints());
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
        updateGlobalVtkPointsImpl(vtk);

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
        updateGlobalVtkPointsImpl(vtk);

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
}

void QRenderWindow::setCurEdgeRender(bool edge_render)
{
    edge_render_ = edge_render;
    emit curEdgeRenderChanged();
}

bool QRenderWindow::getCurEdgeRender()
{
    return this->edge_render_;
}

bool QRenderWindow::getIsEdgeRender(Data& vtk, Index component_id)
{
    if (component_id < 0) return false;

    if (vtk.mesh_actor_manager_ && vtk.mesh_actor_manager_->hasComponent(component_id)) {
        return vtk.mesh_actor_manager_->getIsEdgeRender(component_id);
    }
    if (vtk.geometry_actor_manager_ && vtk.geometry_actor_manager_->hasComponent(component_id)) {
        return vtk.geometry_actor_manager_->getIsEdgeRender(component_id);
    }

    spdlog::error("QRenderWindow::getIsEdgeRender: error getting edge render mode");
    return false;
}

void QRenderWindow::setSelectComponent(Index component_id)
{
    dispatch_async([component_id, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        if (!vtk)
            return;

        this->cur_component_id_ = component_id;
        this->setCurEdgeRender(this->getIsEdgeRender(*vtk, component_id));
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

void QRenderWindow::setScaleBarVisible(bool on)
{
    dispatch_async([on](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        vtk->scale_bar_axis_->SetVisibility(on);
        });
}

void QRenderWindow::setEdgeRender(Index model_id, bool is_render)
{
    dispatch_async([model_id, is_render, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        vtk->mesh_actor_manager_->setRenderEdge(model_id, is_render);

        auto component_ids = model_query_->getComponentIds(model_id);
        for (Index component_id : component_ids) {
            vtk->mesh_actor_manager_->setRenderEdge(component_id, is_render);
            vtk->geometry_actor_manager_->setRenderEdge(component_id, is_render);
        }

        this->setCurEdgeRender(is_render);
    });
}

void QRenderWindow::setComponentEdgeRender(Index component_id, bool is_render)
{
    dispatch_async([component_id, is_render, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);

        if (vtk->mesh_actor_manager_) {
            vtk->mesh_actor_manager_->setRenderEdge(component_id, is_render);
        }

        if (vtk->geometry_actor_manager_) {
            vtk->geometry_actor_manager_->setRenderEdge(component_id, is_render);
        }

        this->setCurEdgeRender(is_render);
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
    QString attr_name,
    int mode,
    QVariantMap args)
{
    dispatch_async([this, attr_name, mode, args](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
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
        if (vtk->mesh_actor_manager_ && vtk->mesh_actor_manager_->getCount(cur_component_id_)) {
            vtk->mesh_actor_manager_->setAttriMode(
                cur_component_id_,
                attr_name.toStdString(),
                mode_enum,
                std_args);
        }
        spdlog::info("-----setAttriMode:" + attr_name.toStdString());
    });
}

void QRenderWindow::cancelAttri()
{
    dispatch_async([this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        if (vtk->mesh_actor_manager_ && vtk->mesh_actor_manager_->getCount(cur_component_id_)) {
            vtk->mesh_actor_manager_->cancelAttri(
                cur_component_id_);
        }
        spdlog::info("--------cancelAttri-----------");
    });
}

vtkStandardNewMacro(QRenderWindow::Data);
