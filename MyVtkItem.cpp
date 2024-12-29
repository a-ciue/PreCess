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
    for (int i = 0; i < 3; i++) {
        vtk->renderer[i]->SetBackground(0.5, 0.5, 0.7);
        vtk->renderer[i]->SetBackground2(0.7, 0.7, 0.7);
        vtk->renderer[i]->SetGradientBackground(true);
    }

    vtk->styles[0] = vtk->faceStyle;
    vtk->styles[1] = vtk->edgeStyle;
    vtk->styles[2] = vtk->blockStyle;
    vtk->styles[3] = vtk->groupStyle;

    vtk->blockStyle->SetSelector(std::make_unique<ActorSelectorHighlight>(vtk->renderer[1]));
    vtk->blockStyle->SetDefaultRenderer(vtk->renderer[1]);
    vtk->groupStyle->SetSelector(std::make_unique<ActorSelectorHighlight>(vtk->renderer[2]));
    vtk->groupStyle->SetDefaultRenderer(vtk->renderer[2]);
    vtk->faceStyle->SetSelector(std::make_unique<SingleFaceSelectorHighlight>(vtk->renderer[0]));
    vtk->faceStyle->SetDefaultRenderer(vtk->renderer[0]);
    vtk->edgeStyle->SetSelector(std::make_unique<SingleEdgeSelectorHighlight>(vtk->renderer[0]));
    vtk->edgeStyle->SetDefaultRenderer(vtk->renderer[0]);

    cur_actor_ = vtk->actor.get();

    return vtk;
}

void MyVtkItem::destroyingVTK(vtkRenderWindow* renderWindow, vtkUserData userData)
{
    auto* vtk = Data::SafeDownCast(userData);
    if (vtk->curRenderer) {
        _camera->DeepCopy(vtk->curRenderer->GetActiveCamera());
    }
}

void MyVtkItem::resetCamera()
{
    dispatch_async([this](vtkRenderWindow* renderWindow, vtkUserData userData) {
        auto* vtk = Data::SafeDownCast(userData);
        if (vtk->curRenderer) {
            vtk->curRenderer->ResetCamera();
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
            QQuickVTKItem::event(QScopedPointer<QMouseEvent>(_click.take()).get());
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

void MyVtkItem::onModelInited(const std::unordered_map<int, std::unique_ptr<Patch>>* patches,
        const std::unordered_map<int, std::unique_ptr<Block>>* blocks,
        const std::unordered_map<int, std::unique_ptr<Group>>* groups)
{
    dispatch_async([patches, blocks, groups, this](vtkRenderWindow* renderWindow, vtkUserData userData)->void {
        Data* vtk = Data::SafeDownCast(userData);
        vtk->actor = std::make_unique<ModelActor>(*patches, *blocks, *groups);
        cur_actor_ = vtk->actor.get();

        vtk->actor->bind_renderer(vtk->renderer[0], ModelActor::RenderMode::Face);
        vtk->actor->bind_renderer(vtk->renderer[1], ModelActor::RenderMode::Block);
        vtk->actor->bind_renderer(vtk->renderer[2], ModelActor::RenderMode::Group);

        resetCamera();
        });
    
}

void MyVtkItem::blocksMerged(const std::vector<int>& block_ids, int father_block, const std::unordered_set<int>& father_block_patches)
{
    
    dispatch_async([block_ids, father_block, father_block_patches, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        vtk->actor->merge_blocks(block_ids, father_block, father_block_patches);
        });
    
}

void MyVtkItem::groupUpdated(int group_id, const std::unordered_set<int>& group_blocks)
{
    dispatch_async([group_id, group_blocks, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        vtk->actor->update_group(group_id, group_blocks);
        });
}

void MyVtkItem::groupMerged(const std::vector<int>& group_ids, int father_group, const std::unordered_set<int>& father_group_blocks)
{
    dispatch_async([group_ids, father_group, father_group_blocks, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        vtk->actor->merge_groups(group_ids, father_group, father_group_blocks);
        });
}

void MyVtkItem::patchUpdated(int patch_id, const std::vector<std::array<double, 3>>& points, const std::vector<std::array<int, 3>>& triangles)
{
    dispatch_async([patch_id, points, triangles, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        vtk->actor->update_patch(patch_id, points, triangles);
        });
}

void MyVtkItem::blockUpdated(int block_id, const std::unordered_set<int>& block_patches)
{
    dispatch_async([block_id, block_patches, this](vtkRenderWindow* renderWindow, vtkUserData userData) ->void {
        Data* vtk = Data::SafeDownCast(userData);
        vtk->actor->update_block(block_id, block_patches);;
        });
}

std::vector<int> MyVtkItem::selectedIDs()
{
	if (cur_style_)
	{
        return cur_style_->GetSelectedIDs(cur_actor_, select_mode_);
	}
    return {};
}

void MyVtkItem::changeRenderer(QString renderMode)
{
    int renderIdx = 0;
    if (renderMode == "Face") {
        renderIdx = 0;
    } else if (renderMode == "Block") {
        renderIdx = 1;
    } else if (renderMode == "Group") {
        renderIdx = 2;
    } else {
        std::cerr << "invalid renderMode in MyVtkItem::changeRenderer" << std::endl;
        return;
    }

    dispatch_async([this, renderIdx](vtkRenderWindow* renderWindow, vtkUserData userData) {
        Data* vtk = Data::SafeDownCast(userData);

        if (vtk->curRenderer) {
            renderWindow->RemoveRenderer(vtk->curRenderer);
        }

        vtk->curRenderer = vtk->renderer[renderIdx];

        renderWindow->AddRenderer(vtk->curRenderer);

        resetCamera();
    });
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

void MyVtkItem::changeEdgeRender(QString renderMode, bool render)
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

    dispatch_async([this, mode, render](vtkRenderWindow* renderWindow, vtkUserData userData) {
        Data* vtk = Data::SafeDownCast(userData);

        if (vtk->actor)
            vtk->actor->render_edge(mode, render);
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
