#include "MyVtkItem.h"
#include "ModelUtil.h"
#include "ToolMesh.h"
#include "SelectManager.h"
QRenderWindow::QRenderWindow()
{
    connect(this, &QQuickItem::widthChanged, this, &QRenderWindow::resetCamera);
    connect(this, &QQuickItem::heightChanged, this, &QRenderWindow::resetCamera);
    selectManager_ = std::make_unique<SelectManager>();
    edge_render_ = false;
}

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
    
        /*vtk->style_
    vtk->styles[0] = vtk->faceStyle;
    vtk->styles[1] = vtk->edgeStyle;
    vtk->styles[2] = vtk->blockStyle;
    vtk->styles[3] = vtk->groupStyle;

    vtk->blockStyle->SetSelector(std::make_unique<BlockSelectorHighlight>(vtk->renderer));
    vtk->blockStyle->SetDefaultRenderer(vtk->renderer);
    vtk->groupStyle->SetSelector(std::make_unique<BlockSelectorHighlight>(vtk->renderer));
    vtk->groupStyle->SetDefaultRenderer(vtk->renderer);
    vtk->faceStyle->SetSelector(std::make_unique<SingleFaceSelectorHighlight>(vtk->renderer));
    vtk->faceStyle->SetDefaultRenderer(vtk->renderer);
    vtk->edgeStyle->SetSelector(std::make_unique<SingleEdgeSelectorHighlight>(vtk->renderer));
    vtk->edgeStyle->SetDefaultRenderer(vtk->renderer);*/
    vtk->style_->SetSelectManager(this->selectManager_.get());
    selectManager_->bindRenderer(vtk->renderer_);
    vtk->style_->SetDefaultRenderer(vtk->renderer_);
    renderWindow->GetInteractor()->SetInteractorStyle(vtk->style_);

    renderWindow->AddRenderer(vtk->renderer_);
    this->data_= vtk.GetPointer();

    return vtk;
}

void QRenderWindow::destroyingVTK(vtkRenderWindow* renderWindow, vtkUserData userData)
{
    auto* vtk = Data::SafeDownCast(userData);
    if (vtk->renderer_) {
        _camera->DeepCopy(vtk->renderer_->GetActiveCamera());
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
    ev->accept();
    return true;
}


void QRenderWindow::deleteModel(Index model_id)
{
    dispatch_async([model_id, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        vtk->models_.erase(model_id);
        });
}

void QRenderWindow::onModelChanged(Index model_id)
{
    dispatch_async([model_id, this](vtkRenderWindow* renderWindow, vtkUserData userData) -> void {
        Data* vtk = Data::SafeDownCast(userData);

        std::optional model_data = model_query_->getModelData(model_id);
        if (model_data)
        {
            if (!vtk->models_.count(model_id))
                vtk->models_[model_id] = std::make_unique<ModelActor>(vtk->renderer_, edge_render_, renderMode_);
            vtk->models_[model_id]->loadModelData(*model_data);
            vtk->models_[model_id]->setRenderMode(renderMode_);
        }
    });
}

void QRenderWindow::setVisibility(Index model_id, bool visibility)
{
    dispatch_async([model_id,visibility, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        selectManager_->clearSelection();
        vtk->models_[model_id]->setVisibility(visibility);
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

void QRenderWindow::setSelectModel(Index model_id)
{
    dispatch_async([model_id, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        selectManager_->bindRenderer(vtk->renderer_);
        this->cur_actor_id_ = model_id;
        if (vtk->models_.count(model_id))
            selectManager_->setSelectActor(vtk->models_[model_id].get());
        else
            selectManager_->setSelectActor(nullptr);
        });
}

void QRenderWindow::setSelectMode(QString select_mode)
{
    dispatch_async([select_mode, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        
        if(select_mode == "Face"){
            select_mode_ = SelectMode::Face;
        }
        else if(select_mode == "Edge"){
            select_mode_ = SelectMode::Edge;
        }
        else if(select_mode == "Block"){
            select_mode_ = SelectMode::Block;
        }
        else{
            select_mode_ = SelectMode::None;
        }
        selectManager_->setSelectMode(select_mode_);        
        });
}

void QRenderWindow::clearSelection()
{
    this->selectManager_->clearSelection();
}

void QRenderWindow::setRenderMode(QString render_mode)
{

    dispatch_async([render_mode, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        if (render_mode == "Face") {
            this->renderMode_ = RenderMode::Face;
            for (auto&& [modelName, modelActor] : vtk->models_) {
                modelActor->setRenderMode(this->renderMode_);
            }
        }
        else if (render_mode == "Block") {
            this->renderMode_ = RenderMode::Block;
            for (auto&& [modelName, modelActor] : vtk->models_) {
                modelActor->setRenderMode(this->renderMode_);
            }
        }
        else {
            std::cout << "rendermode error!" << endl;
        }
        
        });
}

void QRenderWindow::setEdgeRender(bool is_render)
{
    dispatch_async([is_render, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        for (auto&& [modelName, modelActor] : vtk->models_) {
            modelActor->setRenderEdge(is_render);
        }
        });
}

//void QRenderWindow::changeEdgeRender(QString model_name, QString renderMode, bool render)
//{
//    ModelActor::RenderMode mode {};
//    if (renderMode == "Face") {
//        mode = ModelActor::RenderMode::Face;
//    } else if (renderMode == "Block") {
//        mode = ModelActor::RenderMode::Block;
//    } else if (renderMode == "Group") {
//        mode = ModelActor::RenderMode::Group;
//    } else {
//        std::cerr << "invalid renderMode in QRenderWindow::changeEdgeRenderer" << std::endl;
//        return;
//    }
//
//    dispatch_async([model_name,this, mode, render](vtkRenderWindow* renderWindow, vtkUserData userData) {
//        Data* vtk = Data::SafeDownCast(userData);
//
//        if (vtk->actor_[model_name])
//            vtk->actor_[model_name]->render_edge(mode, render);
//    });
//}

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
//        vtk->actor_[model_name] = std::make_unique<ModelActor>(*patches, *blocks, *groups);
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
//        this->renderMode_ = ModelActor::RenderMode::Face;
//    }
//    else if (renderMode == "Block") {
//        this->renderMode_ = ModelActor::RenderMode::Block;
//    }
//    else if (renderMode == "Group") {
//        this->renderMode_ = ModelActor::RenderMode::Group;
//    }
//    else {
//        std::cerr << "invalid renderMode in QRenderWindow::changeRenderer" << std::endl;
//        return;
//    }
//
//    dispatch_async([this](vtkRenderWindow* renderWindow, vtkUserData userData) {
//        Data* vtk = Data::SafeDownCast(userData);
//
//        if (this->renderMode_ == ModelActor::RenderMode::Face)
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
//        else if (this->renderMode_ == ModelActor::RenderMode::Block)
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
//        else if (this->renderMode_ == ModelActor::RenderMode::Group)
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

