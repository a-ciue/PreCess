#include "MyVtkItem.h"
#include "ModelUtil.h"
#include "ToolMesh.h"

QRenderWindow::QRenderWindow()
{
    connect(this, &QQuickItem::widthChanged, this, &QRenderWindow::resetCamera);
    connect(this, &QQuickItem::heightChanged, this, &QRenderWindow::resetCamera);
}

QQuickVTKItem::vtkUserData QRenderWindow::initializeVTK(vtkRenderWindow* renderWindow)
{
    vtkNew<Data> vtk;

    // Note:  It is okay to store some non-graphical VTK objects in the QQuickVTKItem instead of the
    // vtkUserData but ONLY if they are accessed from the qml-render-thread. (i.e. only in the
    // initializeVTK, destroyingVTK or dispatch_async methods)
    // vtk->renderer->GetActiveCamera()->DeepCopy(_camera);

        vtk->renderer->SetBackground(0.5, 0.5, 0.7);
        vtk->renderer->SetBackground2(0.7, 0.7, 0.7);
        vtk->renderer->SetGradientBackground(true);
    

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
    vtk->edgeStyle->SetDefaultRenderer(vtk->renderer);
    renderWindow->AddRenderer(vtk->renderer);
    this->data_= vtk.GetPointer();

    return vtk;
}

void QRenderWindow::destroyingVTK(vtkRenderWindow* renderWindow, vtkUserData userData)
{
    auto* vtk = Data::SafeDownCast(userData);
    if (vtk->renderer) {
        _camera->DeepCopy(vtk->renderer->GetActiveCamera());
    }
}

void QRenderWindow::resetCamera()
{
    dispatch_async([this](vtkRenderWindow* renderWindow, vtkUserData userData) {
        auto* vtk = Data::SafeDownCast(userData);
        if (vtk->renderer) {
            vtk->renderer->ResetCamera();
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

void QRenderWindow::onModelInited(QString model_name,const std::unordered_map<int, std::unique_ptr<Patch>>* patches,
        const std::unordered_map<int, std::unique_ptr<Block>>* blocks,
        const std::unordered_map<int, std::unique_ptr<Group>>* groups)
{
    dispatch_async([model_name,patches, blocks, groups, this](vtkRenderWindow* renderWindow, vtkUserData userData)->void {
        Data* vtk = Data::SafeDownCast(userData);
        vtk->actor_[model_name] = std::make_unique<ModelActor>(*patches, *blocks, *groups);

        vtk->actor_[model_name]->bind_renderer(vtk->renderer);

        resetCamera();
        });
    
}

void QRenderWindow::blocksMerged(QString model_name,const std::vector<int>& block_ids, int father_block, const std::unordered_set<int>& father_block_patches)
{
    
    dispatch_async([model_name,block_ids, father_block, father_block_patches, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        vtk->actor_[model_name]->merge_blocks(block_ids, father_block, father_block_patches);
        });
    
}

void QRenderWindow::groupUpdated(QString model_name ,int group_id, const std::unordered_set<int>& group_blocks)
{
    dispatch_async([model_name,group_id, group_blocks, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        vtk->actor_[model_name]->update_group(group_id, group_blocks);
        });
}

void QRenderWindow::groupMerged(QString model_name,const std::vector<int>& group_ids, int father_group, const std::unordered_set<int>& father_group_blocks)
{
    dispatch_async([model_name,group_ids, father_group, father_group_blocks, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        vtk->actor_[model_name]->merge_groups(group_ids, father_group, father_group_blocks);
        });
}

void QRenderWindow::patchUpdated(QString model_name,int patch_id, const std::vector<std::array<double, 3>>& points, const std::vector<std::array<int, 3>>& triangles)
{
    dispatch_async([model_name,patch_id, points, triangles, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        vtk->actor_[model_name]->update_patch(patch_id, points, triangles);
        });
}

void QRenderWindow::blockUpdated(QString model_name,int block_id, const std::unordered_set<int>& block_patches)
{
    dispatch_async([model_name,block_id, block_patches, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        vtk->actor_[model_name]->update_block(block_id, block_patches);;
        });
}

Q_INVOKABLE void QRenderWindow::deleteModel(QString model_name)
{
    dispatch_async([model_name, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        vtk->actor_.erase(model_name);
        });
}

Q_INVOKABLE void QRenderWindow::renameModel(QString old_name, QString new_name)
{
    dispatch_async([old_name,new_name,this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        auto it = vtk->actor_.find(old_name);
        if (it != vtk->actor_.end()) {
            // 从映射中移除旧键
            //vtk->actor_.erase(it);

            // 重新插入新的键值对
            //vtk->actor_.emplace(new_name, std::move(it->second));
            
            // 1. 先将 it->second 移动到一个临时变量
            auto actorTemp = std::move(it->second);

            // 2. 再从 map 中删除旧的键
            vtk->actor_.erase(it);

            // 3. 重新插入新的键值对
            vtk->actor_.emplace(new_name, std::move(actorTemp));
        }
        else {
            // 旧键不存在，可以抛出异常或处理错误
            throw std::runtime_error("Old key does not exist");
        }
        });
}

Q_INVOKABLE void QRenderWindow::setVisibility(QString model_name, bool visibility)
{
    dispatch_async([model_name,visibility, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);

        vtk->actor_[model_name]->set_visibility(visibility);
        
        });
}

QSelection* QRenderWindow::selectedIDs()
{

    if (this->cur_style_)
    {
        /* std::vector<ModelActor*> actors;
         int index = 0;
     // 遍历 unordered_map
     for (const auto& pair : data_->actor_ )
     {
         // pair.second 是 std::unique_ptr<ModelActor>
         actors.push_back(pair.second.get());
     }*/
        std::unique_ptr<Selection> data(std::move(cur_style_->GetSelectedIDs(data_->actor_, select_mode_)));

        QSelection* selection = new QSelection(std::move(data));
        QJSEngine::setObjectOwnership(selection, QJSEngine::JavaScriptOwnership);
        return selection;
    }
    return nullptr;
}


Q_INVOKABLE void QRenderWindow::changeRenderer(QString renderMode)
{
    if (renderMode == "Face") {
        this->renderMode_ = ModelActor::RenderMode::Face;
    }
    else if (renderMode == "Block") {
        this->renderMode_ = ModelActor::RenderMode::Block;
    }
    else if (renderMode == "Group") {
        this->renderMode_ = ModelActor::RenderMode::Group;
    }
    else {
        std::cerr << "invalid renderMode in QRenderWindow::changeRenderer" << std::endl;
        return;
    }

    dispatch_async([this](vtkRenderWindow* renderWindow, vtkUserData userData) {
        Data* vtk = Data::SafeDownCast(userData);

        if (this->renderMode_ == ModelActor::RenderMode::Face)
        {
            bindStyle("Face");
            for (auto&& [modelName, modelActor] : vtk->actor_)
            {
                //modelActor->face_assembly_->SetVisibility(1);
                //modelActor->face_assembly_->PickableOn();
                //modelActor->block_assembly_->SetVisibility(0);
                //modelActor->block_assembly_->PickableOff();
                //modelActor->group_assembly_->SetVisibility(0);
                modelActor->change_mode("Face");

            }
        }
        else if (this->renderMode_ == ModelActor::RenderMode::Block)
        {
            bindStyle("Block");
            for (auto&& [modelName, modelActor] : vtk->actor_)
            {
                /*cout << "block" << endl;
                modelActor->face_assembly_->SetVisibility(0);
                modelActor->face_assembly_->PickableOff();
                modelActor->block_assembly_->SetVisibility(1);
                modelActor->block_assembly_->PickableOn();
                modelActor->group_assembly_->SetVisibility(0);
                modelActor->group_assembly_->PickableOff();*/
                std::vector<vtkActor*> select = modelActor->get_remove_actor();
                for (auto& actor_ : select)
                {
                    vtk->renderer->RemoveActor(actor_);
                }

                modelActor->change_mode("Block");
            }

        }
        else if (this->renderMode_ == ModelActor::RenderMode::Group)
        {
            bindStyle("Group");
            for (auto&& [modelName, modelActor] : vtk->actor_)
            {
                /*modelActor->face_assembly_->SetVisibility(0);
                modelActor->face_assembly_->PickableOff();
                modelActor->block_assembly_->SetVisibility(0);
                modelActor->block_assembly_->PickableOff();
                modelActor->group_assembly_->SetVisibility(1);
                modelActor->group_assembly_->PickableOn();*/
                std::vector<vtkActor*> select = modelActor->get_remove_actor();
                for (auto& actor_ : select)
                {
                    vtk->renderer->RemoveActor(actor_);
                }
                modelActor->change_mode("Group");
            }
        }

        resetCamera();
        });
    return Q_INVOKABLE void();
}


void QRenderWindow::bindStyle(QString function)
{ 
	int styleIdx {};
    if (function == "Face")
	{
        select_mode_ = SelectMode::Face;
        styleIdx = 0;
    } else if (function == "Edge")
    {
        select_mode_ = SelectMode::Edge;
        styleIdx = 1;
    } else if (function == "Block")
    {
        select_mode_ = SelectMode::Block;
        styleIdx = 2;
    } else if (function == "Group")
    {
        select_mode_ = SelectMode::Group;
        styleIdx = 3;
    } else {
        std::cerr << "invalid Style in QRenderWindow::bindStyle" << std::endl;
        return;
    }

    dispatch_async([styleIdx, this](vtkRenderWindow* renderWindow, vtkUserData userData) {
        Data* vtk = Data::SafeDownCast(userData);

        if (cur_style_)
            cur_style_->ClearSelections();
        renderWindow->GetInteractor()->SetInteractorStyle(vtk->styles[styleIdx]);
        cur_style_ = vtk->styles[styleIdx];
    });
}

void QRenderWindow::unbindStyle()
{
    dispatch_async([this](vtkRenderWindow* renderWindow, vtkUserData userData) {
        Data* vtk = Data::SafeDownCast(userData);

        if (cur_style_)
            cur_style_->ClearSelections();
        renderWindow->GetInteractor()->SetInteractorStyle(vtkNew<vtkInteractorStyleTrackballCamera>());
        cur_style_ = nullptr;
    });
}

void QRenderWindow::changeEdgeRender(QString model_name, QString renderMode, bool render)
{
    ModelActor::RenderMode mode {};
    if (renderMode == "Face") {
        mode = ModelActor::RenderMode::Face;
    } else if (renderMode == "Block") {
        mode = ModelActor::RenderMode::Block;
    } else if (renderMode == "Group") {
        mode = ModelActor::RenderMode::Group;
    } else {
        std::cerr << "invalid renderMode in QRenderWindow::changeEdgeRenderer" << std::endl;
        return;
    }

    dispatch_async([model_name,this, mode, render](vtkRenderWindow* renderWindow, vtkUserData userData) {
        Data* vtk = Data::SafeDownCast(userData);

        if (vtk->actor_[model_name])
            vtk->actor_[model_name]->render_edge(mode, render);
    });
}

void QRenderWindow::setClick()
{
    dispatch_async([this](vtkRenderWindow* renderWindow, vtkUserData userData) {
        Data* vtk = Data::SafeDownCast(userData);

        if (cur_style_) {
            cur_style_->SetClick();
        }
    });
}

vtkStandardNewMacro(QRenderWindow::Data);
