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
    selectManager_->bindRenderer(vtk->renderer_);
    vtk->style_->SetDefaultRenderer(vtk->renderer_);
    renderWindow->GetInteractor()->SetInteractorStyle(vtk->style_);

    renderWindow->AddRenderer(vtk->renderer_);
    this->data_ = vtk.GetPointer();
    vtk->mesh_actor_manager_ = std::make_unique<MeshActorManager>();
    vtk->mesh_actor_manager_->bindGlobalPoints(vtk->global_points_.GetPointer());
    vtk->mesh_actor_manager_->bindRender(vtk->renderer_);
    vtk->spline_actor_manager_ = std::make_unique<GeometryActorManager>();
    vtk->spline_actor_manager_->bindRender(vtk->renderer_);

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
            vtk->spline_actor_manager_->deleteComponent(component_id);
        }

        this->selectManager_->clearSelection();
    });
}

void QRenderWindow::deleteComponent(Index component_id)
{
    dispatch_async([component_id, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);

        if (vtk->mesh_actor_manager_) {
            vtk->mesh_actor_manager_->deleteComponent(component_id);
        }

        if (vtk->spline_actor_manager_) {
            vtk->spline_actor_manager_->deleteComponent(component_id);
        }

        this->selectManager_->clearSelection();
    });
}

void QRenderWindow::updateGlobalVtkPointsImpl(Data* vtk)
{
    if (!vtk || !model_query_)
        return;

    auto pts = model_query_->copyGlobalPoints();

    vtkPoints* gpts = vtk->global_points_.GetPointer();
    gpts->Reset();
    gpts->SetNumberOfPoints((vtkIdType)pts.size());
    for (vtkIdType i = 0; i < (vtkIdType)pts.size(); ++i) {
        gpts->SetPoint(i, pts[(size_t)i].data());
    }
    gpts->Modified();

    // 全局点数变化后同步所有 actor 的 vtkOriginalPointIds 长度
    if (vtk->mesh_actor_manager_) {
        vtk->mesh_actor_manager_->syncOriginalPointIds();
    }

    spdlog::info("[VTK GlobalPoints] updated: N={}", (int)gpts->GetNumberOfPoints());
}

void QRenderWindow::updateGlobalVtkPoints()
{
    dispatch_async([this](vtkRenderWindow*, vtkUserData userData) {
        Data* vtk = Data::SafeDownCast(userData);
        updateGlobalVtkPointsImpl(vtk);
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

        auto component_ids = model_query_->getComponentIds(model_id);
        updateGlobalVtkPointsImpl(vtk);
        // 先删旧 actor
        for (Index component_id : component_ids) {
            vtk->mesh_actor_manager_->deleteComponent(component_id);
            vtk->spline_actor_manager_->deleteComponent(component_id);
        }

        // 再逐 component 重建
        for (Index component_id : component_ids) {
            auto mesh_data = model_query_->getMeshDataByComponent(component_id);
            if (mesh_data) {
                vtk->mesh_actor_manager_->loadMesh(component_id, *mesh_data, vtk->renderer_, ModelRenderMode::Face);
            }

            auto spline_data = model_query_->getSplineDataByComponent(component_id);
            if (spline_data) {
                vtk->spline_actor_manager_->loadSpline(*spline_data);
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
            vtk->mesh_actor_manager_->deleteComponent(component_id);
            if (mesh_data) {
                vtk->mesh_actor_manager_->loadMesh(component_id, *mesh_data, vtk->renderer_, ModelRenderMode::Face);
            }
        }

        if (vtk->spline_actor_manager_) {
            auto spline_data = this->model_query_->getSplineDataByComponent(component_id);
            vtk->spline_actor_manager_->deleteComponent(component_id);
            if (spline_data) {
                vtk->spline_actor_manager_->loadSpline(*spline_data);
            }
        }
    });
}

void QRenderWindow::setVisibility(Index model_id, bool visibility)
{
    dispatch_async([model_id, visibility, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        selectManager_->clearSelection();

        auto component_ids = model_query_->getComponentIds(model_id);
        for (Index component_id : component_ids) {
            vtk->mesh_actor_manager_->setVisibility(component_id, visibility);
            vtk->spline_actor_manager_->setVisibility(component_id, visibility);
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

        if (vtk->spline_actor_manager_) {
            vtk->spline_actor_manager_->setVisibility(component_id, visibility);
        }
    });
}

QSelection* QRenderWindow::selectedIDs()
{
    std::unique_ptr<Selection> data(this->selectManager_->getSelection());
    if (!data) {
        return nullptr;
    }

    data->model_id = this->cur_actor_id_;
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

void QRenderWindow::setCurVertexRender(bool is_render)
{
    //this->vertex_render_ = is_render;
    //emit curVertexRenderChanged();

    //dispatch_async([this, is_render](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
    //    Data* vtk = Data::SafeDownCast(userData);

    //    auto component_ids = model_query_->getComponentIds(this->cur_actor_id_);
    //    for (Index component_id : component_ids) {
    //        vtk->mesh_actor_manager_->setRenderVertex(component_id, is_render);
    //    }
    //});
    // vertex rendering disabled
    if (vertex_render_ != false) {
        vertex_render_ = false;
        emit curVertexRenderChanged();
    }
}

bool QRenderWindow::getCurVertexRender()
{
    //return this->vertex_render_;
    return false;
}

bool QRenderWindow::getIsEdgeRender(Data& vtk, Index model_id)
{
    if (model_id < 0) return false;

    auto component_ids = model_query_->getComponentIds(model_id);
    if (!component_ids.empty()) {
        Index component_id = component_ids.front();

        if (vtk.mesh_actor_manager_ && vtk.mesh_actor_manager_->hasComponent(component_id)) {
            return vtk.mesh_actor_manager_->getIsEdgeRender(component_id);
        }
        if (vtk.spline_actor_manager_ && vtk.spline_actor_manager_->hasComponent(component_id)) {
            return vtk.spline_actor_manager_->getIsEdgeRender(component_id);
        }
    }

    spdlog::error("get is edge render mode error");
    return false;
}

bool QRenderWindow::getIsVertexRender(Data& vtk, Index model_id)
{
    //auto component_ids = model_query_->getComponentIds(model_id);
    //if (!component_ids.empty()) {
    //    Index component_id = component_ids.front();

    //    if (vtk.mesh_actor_manager_ && vtk.mesh_actor_manager_->hasComponent(component_id)) {
    //        return vtk.mesh_actor_manager_->getIsVertexRender(component_id);
    //    }
    //}

    //std::cout << "get is vertex render mode error" << std::endl;
    return false;
}

void QRenderWindow::setSelectModel(Index model_id)
{
    dispatch_async([model_id, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        selectManager_->bindRenderer(vtk->renderer_);
        this->cur_actor_id_ = model_id;
        this->setCurEdgeRender(this->getIsEdgeRender(*vtk, model_id));

        this->vertex_render_ = false;
        emit curVertexRenderChanged();

        auto component_ids = model_query_->getComponentIds(model_id);
        std::shared_ptr<const MeshActor> mesh_actor;

        for (Index component_id : component_ids) {
            if (vtk->mesh_actor_manager_->hasComponent(component_id)) {
                mesh_actor = vtk->mesh_actor_manager_->getComponentActor(component_id);
                break;
            }
        }

        if (mesh_actor)
            selectManager_->setSelectActor(mesh_actor);
        else
            selectManager_->setSelectActor({});
    });
}

void QRenderWindow::setSelectMode(QString select_mode)
{
    dispatch_async([select_mode, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        if (select_mode == "Vertex") {
            select_mode_ = SelectMode::Vertex;
        } else if (select_mode == "Face") {
            select_mode_ = SelectMode::Face;
        } else if (select_mode == "Edge") {
            select_mode_ = SelectMode::Edge;
        } else if (select_mode == "Block") {
            select_mode_ = SelectMode::Block;
        } else if (select_mode == "Solid") {
            select_mode_ = SelectMode::Solid;
        } else {
            select_mode_ = SelectMode::None;
        }
        selectManager_->setSelectMode(select_mode_);
    });
}

void QRenderWindow::clearSelection()
{
    dispatch_async([this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        this->selectManager_->clearSelection();
    });
}

void QRenderWindow::setRenderMode(Index model_id, QString render_mode)
{

    dispatch_async([model_id, render_mode, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        this->selectManager_->clearSelection();

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
            vtk->spline_actor_manager_->setRenderEdge(component_id, is_render);
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

        if (vtk->spline_actor_manager_) {
            vtk->spline_actor_manager_->setRenderEdge(component_id, is_render);
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
        if (vtk->mesh_actor_manager_ && vtk->mesh_actor_manager_->getCount(cur_actor_id_)) {
            vtk->mesh_actor_manager_->setAttriMode(
                cur_actor_id_,
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
        if (vtk->mesh_actor_manager_ && vtk->mesh_actor_manager_->getCount(cur_actor_id_)) {
            vtk->mesh_actor_manager_->cancelAttri(
                cur_actor_id_);
        }
        spdlog::info("--------cancelAttri-----------");
    });
}

vtkStandardNewMacro(QRenderWindow::Data);
