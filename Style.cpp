#include "Style.h"
#include <vtkRenderWindowInteractor.h>
#include "Model.h"

vtkStandardNewMacro(MouseInteractorHighLightActor);
void MouseInteractorHighLightActor::OnLeftButtonUp()
{
    if (selector_ != nullptr && click_)
    {
        click_ = false;
        int pos[2];
        this->GetInteractor()->GetEventPosition(pos);
        OnSelect(pos[0], pos[1]);
    }
    vtkInteractorStyleTrackballCamera::OnLeftButtonUp();
}

void MouseInteractorHighLightActor::OnSelect(double posx, double posy)
{
    selector_->select(posx, posy);
}

void MouseInteractorHighLightActor::SetModel(Model* model)
{
    model_ = model;
}

void MouseInteractorHighLightActor::SetClick()
{
    click_ = true;
}

void MouseInteractorHighLightActor::ClearSelections()
{
    selector_->clear();
}

void MouseInteractorHighLightActor::SetSelector(std::unique_ptr<ActorSelectorHighlight> selector)
{
    selector_ = std::move(selector);
}


void MouseInteractorHighLightActor::OnCommitMergeBlocks()
{
    if (selector_ != nullptr)
    {
        auto actors = selector_->get();
        std::vector<int> block_ids;
        for (auto actor : actors)
        {
            block_ids.push_back(model_->actor().block_actor_id(actor));
        }
        model_->merge_blocks(block_ids);
    }
}

void MouseInteractorHighLightActor::OnCommitMergeGroups()
{
    if (selector_ != nullptr)
    {
        auto actors = selector_->get();
        std::vector<int> group_ids;
        for (auto actor : actors)
        {
            group_ids.push_back(model_->actor().group_actor_id(actor));
        }
        model_->merge_groups(group_ids);
    }
}

vtkStandardNewMacro(MouseInteractorHighLightFace);
void MouseInteractorHighLightFace::OnLeftButtonUp()
{
    if (selector_ != nullptr && click_)
    {
        click_ = false;
        int pos[2];
        this->GetInteractor()->GetEventPosition(pos);
        selector_->select(pos[0], pos[1]);
    }
    vtkInteractorStyleTrackballCamera::OnLeftButtonUp();
}

void MouseInteractorHighLightFace::SetModel(Model* model)
{
    model_ = model;
}

void MouseInteractorHighLightFace::SetClick()
{
    click_ = true;
}

void MouseInteractorHighLightFace::ClearSelections()
{
    selector_->clear();
}

void MouseInteractorHighLightFace::SetSelector(std::unique_ptr<SingleFaceSelectorHighlight> selector)
{
    selector_ = std::move(selector);
}

void MouseInteractorHighLightFace::OnCommitSplitFace()
{
    if (selector_ != nullptr)
    {
        auto selected_face = selector_->get();
        if (selected_face.has_value())
        {
            int patch_id = model_->actor().patch_actor_id(selected_face->patch_actor);
            model_->split_face(patch_id, selected_face->local_id);
        }
    }
}

vtkStandardNewMacro(MouseInteractorHighLightEdge);
void MouseInteractorHighLightEdge::OnLeftButtonUp()
{
    if (selector_ != nullptr && click_)
    {
        click_ = false;
        int pos[2];
        this->GetInteractor()->GetEventPosition(pos);
        selector_->select(pos[0], pos[1]);
    }
    vtkInteractorStyleTrackballCamera::OnLeftButtonUp();
}

void MouseInteractorHighLightEdge::SetModel(Model* model)
{
    model_ = model;
}

void MouseInteractorHighLightEdge::SetClick()
{
    click_ = true;
}

void MouseInteractorHighLightEdge::ClearSelections()
{
    selector_->clear();
}


void MouseInteractorHighLightEdge::SetSelector(std::unique_ptr<SingleEdgeSelectorHighlight> selector)
{
    selector_ = std::move(selector);
}


void MouseInteractorHighLightEdge::OnCommitSplitEdge()
{
    if (selector_ != nullptr)
    {
        auto selected_edge = selector_->get();
        if (selected_edge.has_value())
        {
            int patch_id = model_->actor().patch_actor_id(selected_edge->actor);
            model_->split_edge(patch_id, selected_edge->v_local_id);
        }
    }
}

