#include "QRenderWindow.h"
#include "renderStrategy/AttributeCommon.h"
#include "MeshActorManager.h"
#include "QModelQuery.h"
#include "QRenderWindowStyle.h"
#include "QSelection.h"
#include "SelectManager.h"
#include "GeometrySelectManager.h"
#include "Selection.h"
#include "GeometryActorManager.h"
#include "GeometryDataVtk.h"

#include <spdlog/spdlog.h>
#include <vtkDoubleArray.h>
#include <vtkCallbackCommand.h>
#include <vtkDisplaySizedImplicitPlaneRepresentation.h>
#include <vtkDisplaySizedImplicitPlaneWidget.h>
#include <vtkObjectFactory.h>
#include <vtkPlane.h>
QRenderWindow::QRenderWindow()
{
    connect(this, &QQuickItem::widthChanged, this, &QRenderWindow::resetCamera);
    connect(this, &QQuickItem::heightChanged, this, &QRenderWindow::resetCamera);
    selectManager_ = std::make_unique<SelectManager>();
    geometrySelectManager_ = std::make_unique<GeometrySelectManager>();
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

    vtk->renderer_->SetBackground(0.5, 0.5, 0.7);
    vtk->renderer_->SetBackground2(0.7, 0.7, 0.7);
    vtk->renderer_->SetGradientBackground(true);

    vtk->style_->SetSelectManager(this->selectManager_.get());
    vtk->style_->SetGeometrySelectManager(this->geometrySelectManager_.get());
    selectManager_->bindRenderer(vtk->renderer_);
    geometrySelectManager_->bindRenderer(vtk->renderer_);
    vtk->style_->SetDefaultRenderer(vtk->renderer_);
    renderWindow->GetInteractor()->SetInteractorStyle(vtk->style_);

    renderWindow->AddRenderer(vtk->renderer_);
    this->data_ = vtk.GetPointer();
    vtk->mesh_actor_manager_ = std::make_unique<MeshActorManager>(vtk->global_points_.GetPointer());
    vtk->mesh_actor_manager_->bindRender(vtk->renderer_);
    vtk->geometry_actor_manager_ = std::make_unique<GeometryActorManager>();
    vtk->geometry_actor_manager_->bindRender(vtk->renderer_);

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

        if (data_)
            data_->style_->SetClick();
        auto e = static_cast<QMouseEvent*>(ev);
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

        this->selectManager_->clearSelection();
        this->geometrySelectManager_->clearSelection();
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

        this->selectManager_->clearSelection();
        this->geometrySelectManager_->clearSelection();
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
    arr->SetArray(const_cast<double*>(pts.data()->data()), totalVals, 1);

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

        auto component_ids = model_query_->getComponentIds(model_id);
        updateGlobalVtkPointsImpl(vtk);

        for (Index component_id : component_ids) {
            auto mesh_data = model_query_->getMeshDataByComponent(component_id);
            if (mesh_data) {
                vtk->mesh_actor_manager_->loadMesh(component_id, *mesh_data, vtk->renderer_, ModelRenderMode::Face);
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

        if (!this->model_query_)
            return;

        updateGlobalVtkPointsImpl(vtk);

        if (vtk->mesh_actor_manager_) {
            auto mesh_data = this->model_query_->getMeshDataByComponent(component_id);
            if (mesh_data) {
                vtk->mesh_actor_manager_->loadMesh(component_id, *mesh_data, vtk->renderer_, ModelRenderMode::Face);
            }
        }

        if (vtk->geometry_actor_manager_) {
            auto geometry_data = this->model_query_->getGeometryVtkDataByComponent(component_id);
            if (geometry_data) {
                vtk->geometry_actor_manager_->loadGeometry(*geometry_data);
            }
        }
    });
}

void QRenderWindow::setVisibility(Index model_id, bool visibility)
{
    dispatch_async([model_id, visibility, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        selectManager_->clearSelection();
        geometrySelectManager_->clearSelection();

        auto component_ids = model_query_->getComponentIds(model_id);
        for (Index component_id : component_ids) {
            vtk->mesh_actor_manager_->setVisibility(component_id, visibility);
            vtk->geometry_actor_manager_->setVisibility(component_id, visibility);
        }
    });
}

void QRenderWindow::setComponentVisibility(Index component_id, bool visibility)
{
    dispatch_async([component_id, visibility](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);

        if (vtk->mesh_actor_manager_) {
            vtk->mesh_actor_manager_->setVisibility(component_id, visibility);
        }

        if (vtk->geometry_actor_manager_) {
            vtk->geometry_actor_manager_->setVisibility(component_id, visibility);
        }
    });
}

QSelection* QRenderWindow::selectedIDs()
{
    std::unique_ptr<Selection> data;
    if (geom_select_mode_ != SelectMode::None) {
        data = this->geometrySelectManager_->getSelection();
    } else {
        data = this->selectManager_->getSelection();
    }
    if (!data) {
        return nullptr;
    }

    data->component_id = this->cur_component_id_;
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

    spdlog::error("get is edge render mode error");
    return false;
}

void QRenderWindow::setSelectComponent(Index component_id)
{
    dispatch_async([component_id, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        this->cur_component_id_ = component_id;
        this->setCurEdgeRender(this->getIsEdgeRender(*vtk, component_id));

        std::shared_ptr<const MeshActor> mesh_actor;
        if (vtk->mesh_actor_manager_->hasComponent(component_id))
            mesh_actor = vtk->mesh_actor_manager_->getComponentActor(component_id);

        if (mesh_actor)
            selectManager_->setSelectActor(mesh_actor);
        else
            selectManager_->setSelectActor({});

        std::shared_ptr<const GeometryActor> geometry_actor;
        if (vtk->geometry_actor_manager_->hasComponent(component_id))
            geometry_actor = vtk->geometry_actor_manager_->getComponentActor(component_id);

        if (geometry_actor)
            geometrySelectManager_->setSelectActor(geometry_actor);
        else
            geometrySelectManager_->setSelectActor({});
    });
}

void QRenderWindow::setSelectMode(QString select_mode)
{
    dispatch_async([select_mode, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        if (select_mode == "Vertex") {
            select_mode_ = SelectMode::Vertex;
            geom_select_mode_ = SelectMode::None;
        } else if (select_mode == "Face") {
            select_mode_ = SelectMode::Face;
            geom_select_mode_ = SelectMode::None;
        } else if (select_mode == "Edge") {
            select_mode_ = SelectMode::Edge;
            geom_select_mode_ = SelectMode::None;
        } else if (select_mode == "Block") {
            select_mode_ = SelectMode::Block;
            geom_select_mode_ = SelectMode::None;
        } else if (select_mode == "Solid") {
            select_mode_ = SelectMode::Solid;
            geom_select_mode_ = SelectMode::None;
        } else if (select_mode == "GeometryFace") {
            select_mode_ = SelectMode::None;
            geom_select_mode_ = SelectMode::Face;
        } else if (select_mode == "GeometryEdge") {
            select_mode_ = SelectMode::None;
            geom_select_mode_ = SelectMode::Edge;
        } else if (select_mode == "GeometryVertex") {
            select_mode_ = SelectMode::None;
            geom_select_mode_ = SelectMode::Vertex;
        } else if (select_mode == "GeometrySolid") {
            select_mode_ = SelectMode::None;
            geom_select_mode_ = SelectMode::Solid;
        } else {
            select_mode_ = SelectMode::None;
            geom_select_mode_ = SelectMode::None;
        }
        selectManager_->setSelectMode(select_mode_);
        geometrySelectManager_->setSelectMode(geom_select_mode_);
    });
}

void QRenderWindow::clearSelection()
{
    dispatch_async([this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        this->selectManager_->clearSelection();
        this->geometrySelectManager_->clearSelection();
    });
}

void QRenderWindow::setRenderMode(Index model_id, QString render_mode)
{

    dispatch_async([model_id, render_mode, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        this->selectManager_->clearSelection();
        this->geometrySelectManager_->clearSelection();

        auto component_ids = model_query_->getComponentIds(model_id);

        if (render_mode == "Face") {
            for (Index component_id : component_ids) {
                vtk->mesh_actor_manager_->setRenderMode(component_id, ModelRenderMode::Face);
            }
        } else if (render_mode == "Block") {
            for (Index component_id : component_ids) {
                vtk->mesh_actor_manager_->setRenderMode(component_id, ModelRenderMode::Block);
            }
        } else {
            qWarning() << "render mode error!";
        }
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
