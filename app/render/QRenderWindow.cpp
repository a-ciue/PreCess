#include "QRenderWindow.h"
#include "MeshActor.h"
#include "MeshActorManager.h"
#include "QModelQuery.h"
#include "QRenderWindowStyle.h"
#include "QSelection.h"
#include "SelectManager.h"
#include "Selection.h"
#include "SplineActorManager.h"
#include "SplineDataVtk.h"

#include <vtkCallbackCommand.h>
#include <vtkDisplaySizedImplicitPlaneRepresentation.h>
#include <vtkDisplaySizedImplicitPlaneWidget.h>
#include <vtkObjectFactory.h>
#include <vtkPlane.h>
#include <spdlog/spdlog.h>

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
    vtk->mesh_actor_manager_->bindRender(vtk->renderer_);
    vtk->spline_actor_manager_ = std::make_unique<SplineActorManager>();
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
        vtk->mesh_actor_manager_->deleteModel(model_id);
        vtk->spline_actor_manager_->deleteModel(model_id);
        this->selectManager_->clearSelection();
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
        int type = this->model_query_->getModelType(model_id);
        if (this->model_query_->getModelType(model_id) == 0) {
            std::optional mesh_data = model_query_->getMeshData(model_id);
            if (mesh_data) {
                vtk->mesh_actor_manager_->loadModel(model_id, *mesh_data, vtk->renderer_, this->renderMode_);
            }
        } else if (this->model_query_->getModelType(model_id) == 1) {
            std::optional spline_data = model_query_->getSplineData(model_id);
            if (spline_data) {
                vtk->spline_actor_manager_->loadSpline(model_id, *spline_data);
            }
        } else {
            vtk->mesh_actor_manager_->deleteModel(model_id);
            vtk->spline_actor_manager_->deleteModel(model_id);
            spdlog::info("QRenderWindow::onModelChanged: unrecognized model type loaded as empty.");
        }
    });
}

void QRenderWindow::setVisibility(Index model_id, bool visibility)
{
    dispatch_async([model_id, visibility, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        selectManager_->clearSelection();
        vtk->mesh_actor_manager_->setVisibility(model_id, visibility);
        vtk->spline_actor_manager_->setVisibility(model_id, visibility);
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
    this->vertex_render_ = is_render;
    emit curVertexRenderChanged();

    dispatch_async([this, is_render](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);

        vtk->mesh_actor_manager_->setRenderVertex(this->cur_actor_id_, is_render);
    });
}

bool QRenderWindow::getCurVertexRender()
{
    return this->vertex_render_;
}

bool QRenderWindow::getIsEdgeRender(Data& vtk, Index model_id)
{
    if (vtk.mesh_actor_manager_ && vtk.mesh_actor_manager_->getCount(model_id)) {
        return vtk.mesh_actor_manager_->getIsEdgeRender(model_id);
    }
    if (vtk.spline_actor_manager_ && vtk.spline_actor_manager_->getCount(model_id)) {
        return vtk.spline_actor_manager_->getIsEdgeRender(model_id);
    }

    std::cout << "get is edge render mode error" << std::endl;
    return false;
}

bool QRenderWindow::getIsVertexRender(Data& vtk, Index model_id)
{
    if (vtk.mesh_actor_manager_ && vtk.mesh_actor_manager_->getCount(model_id)) {
        return vtk.mesh_actor_manager_->getIsVertexRender(model_id);
    }

    std::cout << "get is vertex render mode error" << std::endl;
    return false;
}

void QRenderWindow::setSelectModel(Index model_id)
{
    dispatch_async([model_id, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);
        selectManager_->bindRenderer(vtk->renderer_);
        this->cur_actor_id_ = model_id;
        this->setCurEdgeRender(this->getIsEdgeRender(*vtk, model_id));

        this->vertex_render_ = this->getIsVertexRender(*vtk, model_id);
        emit curVertexRenderChanged();

        if (vtk->mesh_actor_manager_->getCount(model_id))
            selectManager_->setSelectActor(vtk->mesh_actor_manager_->getModelActor(model_id));
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
        if (render_mode == "Face") {
            vtk->mesh_actor_manager_->setRenderMode(model_id, ModelRenderMode::Face);
        } else if (render_mode == "Block") {
            vtk->mesh_actor_manager_->setRenderMode(model_id, ModelRenderMode::Block);
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

vtkStandardNewMacro(QRenderWindow::Data);
