#include "QRenderWindow.h"
#include "SelectManager.h"
#include "QRenderWindowStyle.h"
#include "Selection.h"
#include "MeshActor.h"
#include "SplineDataVtk.h"
#include "SelectManager.h"
#include "QModelQuery.h"
#include "MeshActorManager.h"
#include "SplineActorManager.h"
#include "QSelection.h"
#include <vtkObjectFactory.h>
#include <vtkDisplaySizedImplicitPlaneWidget.h>
#include <vtkDisplaySizedImplicitPlaneRepresentation.h>
#include <vtkCallbackCommand.h>
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
    dispatch_async([model_id, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
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
        if (this->model_query_->getModelType(model_id)==0)
        {
            std::optional mesh_data = model_query_->getMeshData(model_id);
            if (mesh_data)
            {
                vtk->mesh_actor_manager_->loadModel(model_id, *mesh_data, vtk->renderer_, this->renderMode_, 1);
            }
        }
        else if (this->model_query_->getModelType(model_id) == 1)
        {
	        std::optional spline_data = model_query_->getSplineData(model_id);
            if (spline_data)
            {
                vtk->spline_actor_manager_->loadSpline(model_id, *spline_data);
            }
        }
        else
        {
            std::cout << "load model error" << std::endl;
        }

        });
}

void QRenderWindow::setVisibility(Index model_id, bool visibility)
{
    dispatch_async([model_id,visibility, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        selectManager_->clearSelection();
        vtk->mesh_actor_manager_->setVisibility(model_id, visibility);
        vtk->spline_actor_manager_->setVisibility(model_id, visibility);
        });
}

QSelection* QRenderWindow::selectedIDs()
{
    std::unique_ptr<Selection> data(this->selectManager_->getSelection());
    if (!data)
    {
        return nullptr;
    }

    data->model_id= this->cur_actor_id_;
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

bool QRenderWindow::getIsEdgeRender(Data& vtk, Index model_id)
{
    if (vtk.mesh_actor_manager_ && vtk.mesh_actor_manager_->getCount(model_id))
    {
        return vtk.mesh_actor_manager_->getIsEdgeRender(model_id);
    }
    if (vtk.spline_actor_manager_ && vtk.spline_actor_manager_->getCount(model_id))
    {
        return vtk.spline_actor_manager_->getIsEdgeRender(model_id);
    }

    std::cout << "get is edge render mode error" << std::endl;
    return false;
}

void QRenderWindow::setSelectModel(Index model_id)
{
    dispatch_async([model_id, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        selectManager_->bindRenderer(vtk->renderer_);
        this->cur_actor_id_ = model_id;
	this->setCurEdgeRender(this->getIsEdgeRender(*vtk, model_id));
        if (vtk->mesh_actor_manager_->getCount(model_id))
            selectManager_->setSelectActor(vtk->mesh_actor_manager_->getModelActor(model_id));
        else
            selectManager_->setSelectActor({});
        });
}

void QRenderWindow::setSelectMode(QString select_mode)
{
    dispatch_async([select_mode, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        if (select_mode == "Vertex") {
            select_mode_ = SelectMode::Vertex;
        }
        else if(select_mode == "Face"){
            select_mode_ = SelectMode::Face;
        }
        else if(select_mode == "Edge"){
            select_mode_ = SelectMode::Edge;
        }
        else if(select_mode == "Block"){
            select_mode_ = SelectMode::Block;
        } else if (select_mode == "Solid"){
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

    dispatch_async([model_id,render_mode, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        this->selectManager_->clearSelection();
        if (render_mode == "Face") {
            vtk->mesh_actor_manager_->setRenderMode(model_id, ModelRenderMode::Face);
        }
        else if (render_mode == "Block") {
            vtk->mesh_actor_manager_->setRenderMode(model_id, ModelRenderMode::Block);
        }
        else {
            qWarning() << "render mode error!";
        }
        
        });
}

void QRenderWindow::setEdgeRender(Index model_id, bool is_render)
{
    dispatch_async([model_id,is_render, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
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

//void QRenderWindow::onModelInited(QString model_name,const std::unordered_map<int, std::unique_ptr<Patch>>* patches,
//        const std::unordered_map<int, std::unique_ptr<Block>>* blocks,
//        const std::unordered_map<int, std::unique_ptr<Group>>* groups)
//{
//    dispatch_async([model_name,patches, blocks, groups, this](vtkRenderWindow* renderWindow, vtkUserData userData)->void {
//        Data* vtk = Data::SafeDownCast(userData);
//        vtk->actor_[model_name] = std::make_unique<MeshActor>(*patches, *blocks, *groups);
//
//        vtk->actor_[model_name]->bind_renderer(vtk->renderer);
//
//        resetCamera();
//        });
//    
//}
//
//void QRenderWindow::blocksMerged(QString model_name,const std::vector<int>& block_ids, int father_block, const std::unordered_set<int>& father_block_patches)
//{
//    
//    dispatch_async([model_name,block_ids, father_block, father_block_patches, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
//        Data* vtk = Data::SafeDownCast(userData);
//        vtk->actor_[model_name]->merge_blocks(block_ids, father_block, father_block_patches);
//        });
//    
//}
//
//void QRenderWindow::groupUpdated(QString model_name ,int group_id, const std::unordered_set<int>& group_blocks)
//{
//    dispatch_async([model_name,group_id, group_blocks, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
//        Data* vtk = Data::SafeDownCast(userData);
//        vtk->actor_[model_name]->update_group(group_id, group_blocks);
//        });
//}
//
//void QRenderWindow::groupMerged(QString model_name,const std::vector<int>& group_ids, int father_group, const std::unordered_set<int>& father_group_blocks)
//{
//    dispatch_async([model_name,group_ids, father_group, father_group_blocks, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
//        Data* vtk = Data::SafeDownCast(userData);
//        vtk->actor_[model_name]->merge_groups(group_ids, father_group, father_group_blocks);
//        });
//}
//
//void QRenderWindow::patchUpdated(QString model_name,int patch_id, const std::vector<std::array<double, 3>>& points, const std::vector<std::array<int, 3>>& triangles)
//{
//    dispatch_async([model_name,patch_id, points, triangles, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
//        Data* vtk = Data::SafeDownCast(userData);
//        vtk->actor_[model_name]->update_patch(patch_id, points, triangles);
//        });
//}
//
//void QRenderWindow::blockUpdated(QString model_name,int block_id, const std::unordered_set<int>& block_patches)
//{
//    dispatch_async([model_name,block_id, block_patches, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
//        Data* vtk = Data::SafeDownCast(userData);
//        vtk->actor_[model_name]->update_block(block_id, block_patches);;
//        });
//}
//Q_INVOKABLE void QRenderWindow::changeRenderer(QString renderMode)
//{
//    if (renderMode == "Face") {
//        this->renderMode_ = MeshActor::RenderMode::Face;
//    }
//    else if (renderMode == "Block") {
//        this->renderMode_ = MeshActor::RenderMode::Block;
//    }
//    else if (renderMode == "Group") {
//        this->renderMode_ = MeshActor::RenderMode::Group;
//    }
//    else {
//        std::cerr << "invalid renderMode in QRenderWindow::changeRenderer" << std::endl;
//        return;
//    }
//
//    dispatch_async([this](vtkRenderWindow* renderWindow, vtkUserData userData) {
//        Data* vtk = Data::SafeDownCast(userData);
//
//        if (this->renderMode_ == MeshActor::RenderMode::Face)
//        {
//            bindStyle("Face");
//            for (auto&& [modelName, modelActor] : vtk->actor_)
//            {
//                //modelActor->face_assembly_->SetVisibility(1);
//                //modelActor->face_assembly_->PickableOn();
//                //modelActor->block_assembly_->SetVisibility(0);
//                //modelActor->block_assembly_->PickableOff();
//                //modelActor->group_assembly_->SetVisibility(0);
//                modelActor->change_mode("Face");
//
//            }
//        }
//        else if (this->renderMode_ == MeshActor::RenderMode::Block)
//        {
//            bindStyle("Block");
//            for (auto&& [modelName, modelActor] : vtk->actor_)
//            {
//                /*cout << "block" << endl;
//                modelActor->face_assembly_->SetVisibility(0);
//                modelActor->face_assembly_->PickableOff();
//                modelActor->block_assembly_->SetVisibility(1);
//                modelActor->block_assembly_->PickableOn();
//                modelActor->group_assembly_->SetVisibility(0);
//                modelActor->group_assembly_->PickableOff();*/
//                std::vector<vtkActor*> select = modelActor->get_remove_actor();
//                for (auto& actor_ : select)
//                {
//                    vtk->renderer->RemoveActor(actor_);
//                }
//
//                modelActor->change_mode("Block");
//            }
//
//        }
//        else if (this->renderMode_ == MeshActor::RenderMode::Group)
//        {
//            bindStyle("Group");
//            for (auto&& [modelName, modelActor] : vtk->actor_)
//            {
//                /*modelActor->face_assembly_->SetVisibility(0);
//                modelActor->face_assembly_->PickableOff();
//                modelActor->block_assembly_->SetVisibility(0);
//                modelActor->block_assembly_->PickableOff();
//                modelActor->group_assembly_->SetVisibility(1);
//                modelActor->group_assembly_->PickableOn();*/
//                std::vector<vtkActor*> select = modelActor->get_remove_actor();
//                for (auto& actor_ : select)
//                {
//                    vtk->renderer->RemoveActor(actor_);
//                }
//                modelActor->change_mode("Group");
//            }
//        }
//
//        resetCamera();
//        });
//    return Q_INVOKABLE void();
//}
//
//
//void QRenderWindow::bindStyle(QString function)
//{ 
//	int styleIdx {};
//    if (function == "Face")
//	{
//        select_mode_ = SelectMode::Face;
//        styleIdx = 0;
//    } else if (function == "Edge")
//    {
//        select_mode_ = SelectMode::Edge;
//        styleIdx = 1;
//    } else if (function == "Block")
//    {
//        select_mode_ = SelectMode::Block;
//        styleIdx = 2;
//    } else if (function == "Group")
//    {
//        select_mode_ = SelectMode::Group;
//        styleIdx = 3;
//    } else {
//        std::cerr << "invalid Style in QRenderWindow::bindStyle" << std::endl;
//        return;
//    }
//
//    dispatch_async([styleIdx, this](vtkRenderWindow* renderWindow, vtkUserData userData) {
//        Data* vtk = Data::SafeDownCast(userData);
//
//        if (cur_style_)
//            cur_style_->ClearSelections();
//        renderWindow->GetInteractor()->SetInteractorStyle(vtk->styles[styleIdx]);
//        cur_style_ = vtk->styles[styleIdx];
//    });
//}
//
//void QRenderWindow::unbindStyle()
//{
//    dispatch_async([this](vtkRenderWindow* renderWindow, vtkUserData userData) {
//        Data* vtk = Data::SafeDownCast(userData);
//
//        if (cur_style_)
//            cur_style_->ClearSelections();
//        renderWindow->GetInteractor()->SetInteractorStyle(vtkNew<vtkInteractorStyleTrackballCamera>());
//        cur_style_ = nullptr;
//    });
//}

