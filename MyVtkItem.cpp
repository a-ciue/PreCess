#include "MyVtkItem.h"
#include "ModelUtil.h"
#include "ToolMesh.h"

MyVtkItem::MyVtkItem()
{
    connect(this, &QQuickItem::widthChanged, this, &MyVtkItem::resetCamera);
    connect(this, &QQuickItem::heightChanged, this, &MyVtkItem::resetCamera);
}

QQuickVTKItem::vtkUserData MyVtkItem::initializeVTK(vtkRenderWindow* renderWindow)
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

    vtk->blockStyle->SetSelector(std::make_unique<ActorSelectorHighlight>(vtk->renderer));
    vtk->blockStyle->SetDefaultRenderer(vtk->renderer);
    vtk->groupStyle->SetSelector(std::make_unique<ActorSelectorHighlight>(vtk->renderer));
    vtk->groupStyle->SetDefaultRenderer(vtk->renderer);
    vtk->faceStyle->SetSelector(std::make_unique<SingleFaceSelectorHighlight>(vtk->renderer));
    vtk->faceStyle->SetDefaultRenderer(vtk->renderer);
    vtk->edgeStyle->SetSelector(std::make_unique<SingleEdgeSelectorHighlight>(vtk->renderer));
    vtk->edgeStyle->SetDefaultRenderer(vtk->renderer);

    this->data_= vtk.GetPointer();

    return vtk;
}

void MyVtkItem::destroyingVTK(vtkRenderWindow* renderWindow, vtkUserData userData)
{
    auto* vtk = Data::SafeDownCast(userData);
    if (vtk->renderer) {
        _camera->DeepCopy(vtk->renderer->GetActiveCamera());
    }
}

void MyVtkItem::resetCamera()
{
    dispatch_async([this](vtkRenderWindow* renderWindow, vtkUserData userData) {
        auto* vtk = Data::SafeDownCast(userData);
        if (vtk->renderer) {
            vtk->renderer->ResetCamera();
        }
        scheduleRender();
    });
}

bool MyVtkItem::event(QEvent* ev)
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

void MyVtkItem::onModelInited(QString model_name,const std::unordered_map<int, std::unique_ptr<Patch>>* patches,
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

void MyVtkItem::blocksMerged(QString model_name,const std::vector<int>& block_ids, int father_block, const std::unordered_set<int>& father_block_patches)
{
    
    dispatch_async([model_name,block_ids, father_block, father_block_patches, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        vtk->actor_[model_name]->merge_blocks(block_ids, father_block, father_block_patches);
        });
    
}

void MyVtkItem::groupUpdated(QString model_name ,int group_id, const std::unordered_set<int>& group_blocks)
{
    dispatch_async([model_name,group_id, group_blocks, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        vtk->actor_[model_name]->update_group(group_id, group_blocks);
        });
}

void MyVtkItem::groupMerged(QString model_name,const std::vector<int>& group_ids, int father_group, const std::unordered_set<int>& father_group_blocks)
{
    dispatch_async([model_name,group_ids, father_group, father_group_blocks, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        vtk->actor_[model_name]->merge_groups(group_ids, father_group, father_group_blocks);
        });
}

void MyVtkItem::patchUpdated(QString model_name,int patch_id, const std::vector<std::array<double, 3>>& points, const std::vector<std::array<int, 3>>& triangles)
{
    dispatch_async([model_name,patch_id, points, triangles, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        vtk->actor_[model_name]->update_patch(patch_id, points, triangles);
        });
}

void MyVtkItem::blockUpdated(QString model_name,int block_id, const std::unordered_set<int>& block_patches)
{
    dispatch_async([model_name,block_id, block_patches, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        vtk->actor_[model_name]->update_block(block_id, block_patches);;
        });
}

std::vector<int> MyVtkItem::selectedIDs()
{
    
        if (this->cur_style_)
	{
            std::vector<ModelActor*> actors;
            int index = 0;
        // ±éÀú unordered_map
        for (const auto& pair : data_->actor_ )
        {
            // pair.second ÊÇ std::unique_ptr<ModelActor>
            actors.push_back(pair.second.get());
        }

        return cur_style_->GetSelectedIDs(actors, select_mode_);
	}
       
	
}


Q_INVOKABLE void MyVtkItem::changeRenderer(QString renderMode)
{
    if (renderMode == "Face") {
        this->renderMode_ = 0;
    }
    else if (renderMode == "Block") {
        this->renderMode_ = 1;
    }
    else if (renderMode == "Group") {
        this->renderMode_ = 2;
    }
    else {
        std::cerr << "invalid renderMode in MyVtkItem::changeRenderer" << std::endl;
        return;
    }

    dispatch_async([this](vtkRenderWindow* renderWindow, vtkUserData userData) {
        Data* vtk = Data::SafeDownCast(userData);

        renderWindow->AddRenderer(vtk->renderer);

        resetCamera();
        });
    return Q_INVOKABLE void();
}

Q_INVOKABLE void MyVtkItem::changeRenderMode(int renderMode)
{
    dispatch_async([renderMode,this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        if (renderMode == 0)
        {
            bindStyle("Face");
            for (auto&& [modelName, modelActor] : vtk->actor_)
            {
                //modelActor->face_assembly_->SetVisibility(1);
                //modelActor->face_assembly_->PickableOn();
                //modelActor->block_assembly_->SetVisibility(0);
                //modelActor->block_assembly_->PickableOff();
                //modelActor->group_assembly_->SetVisibility(0);
                //modelActor->group_assembly_->PickableOff();
                vtk->renderer->AddActor(modelActor->face_assembly_);
                vtk->renderer->RemoveActor(modelActor->group_assembly_);
                vtk->renderer->RemoveActor(modelActor->block_assembly_);
                
            }
        }
        else if (renderMode == 1)
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
                std::vector<vtkActor*> select =modelActor->get_remove_actor();
                for (auto& actor_ : select)
                {
                    vtk->renderer->RemoveActor(actor_);
                }

                vtk->renderer->RemoveActor(modelActor->face_assembly_);
                vtk->renderer->RemoveActor(modelActor->group_assembly_);
                vtk->renderer->AddActor(modelActor->block_assembly_);
            }

        }
        else if (renderMode == 2)
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
                vtk->renderer->RemoveActor(modelActor->face_assembly_);
                vtk->renderer->RemoveActor(modelActor->block_assembly_);
                vtk->renderer->AddActor(modelActor->group_assembly_);
            }
        }
        });
    return Q_INVOKABLE void();
}

void MyVtkItem::bindStyle(QString function)
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
        std::cerr << "invalid Style in MyVtkItem::bindStyle" << std::endl;
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

void MyVtkItem::unbindStyle()
{
    dispatch_async([this](vtkRenderWindow* renderWindow, vtkUserData userData) {
        Data* vtk = Data::SafeDownCast(userData);

        if (cur_style_)
            cur_style_->ClearSelections();
        renderWindow->GetInteractor()->SetInteractorStyle(vtkNew<vtkInteractorStyleTrackballCamera>());
        cur_style_ = nullptr;
    });
}

void MyVtkItem::changeEdgeRender(QString model_name, QString renderMode, bool render)
{
    ModelActor::RenderMode mode {};
    if (renderMode == "Face") {
        mode = ModelActor::RenderMode::Face;
    } else if (renderMode == "Block") {
        mode = ModelActor::RenderMode::Block;
    } else if (renderMode == "Group") {
        mode = ModelActor::RenderMode::Group;
    } else {
        std::cerr << "invalid renderMode in MyVtkItem::changeEdgeRenderer" << std::endl;
        return;
    }

    dispatch_async([model_name,this, mode, render](vtkRenderWindow* renderWindow, vtkUserData userData) {
        Data* vtk = Data::SafeDownCast(userData);

        if (vtk->actor_[model_name])
            vtk->actor_[model_name]->render_edge(mode, render);
    });
}

void MyVtkItem::setClick()
{
    dispatch_async([this](vtkRenderWindow* renderWindow, vtkUserData userData) {
        Data* vtk = Data::SafeDownCast(userData);

        if (cur_style_) {
            cur_style_->SetClick();
        }
    });
}

vtkStandardNewMacro(MyVtkItem::Data);
