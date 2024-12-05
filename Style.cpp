#include "Style.h"

MouseInterActorHighLightActor* MouseInterActorHighLightActor::New()
{
    return new MouseInterActorHighLightActor;
}

void MouseInterActorHighLightActor::OnLeftButtonUp()
{
    if (selector_ != nullptr)
    {
        double pos[2];
        this->GetInteractor()->GetEventPosition(pos);
        selector_->select(pos[0], pos[1]);
    }
    vtkInteractorStyleTrackballCamera::OnLeftButtonUp();
}

void MouseInterActorHighLightActor::SetModel(Model* model)
{
    model = model;
}

void MouseInterActorHighLightActor::SetSelector(std::unique_ptr<ActorSelectorHighlight> selector)
{
    selector_ = std::move(selector);
}


void MouseInterActorHighLightActor::OnCommitMergeBlocks()
{
    if (selector_ != nullptr)
    {
        auto actors = selector_->get();
        std::vector<int> block_ids;
        for (auto actor : actors)
        {
            block_ids.push_back(model->actor().block_actor_id(actor));
        }
        model->merge_blocks(block_ids);
    }
}

void MouseInterActorHighLightActor::OnCommitMergeGroups()
{
    if (selector_ != nullptr)
    {
        auto actors = selector_->get();
        std::vector<int> group_ids;
        for (auto actor : actors)
        {
            group_ids.push_back(model->actor().group_actor_id(actor));
        }
        model->merge_groups(group_ids);
    }
}

void MouseInteractorHighLightFace::OnLeftButtonUp()
{
    if (selector_ != nullptr)
    {
        double pos[2];
        this->GetInteractor()->GetEventPosition(pos);
        selector_->select(pos[0], pos[1]);
    }
    vtkInteractorStyleTrackballCamera::OnLeftButtonUp();
}

void MouseInteractorHighLightFace::SetModel(Model* model)
{
    model = model;
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
            model->split_face(selected_face->actor, selected_face->local_id);
        }
    }
}

void MouseInteractorHighLightEdge::OnLeftButtonUp()
{
    if (selector_ != nullptr)
    {
        double pos[2];
        this->GetInteractor()->GetEventPosition(pos);
        selector_->select(pos[0], pos[1]);
    }
    vtkInteractorStyleTrackballCamera::OnLeftButtonUp();
}

void MouseInteractorHighLightEdge::SetModel(Model* model)
{
    model = model;
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
            model->split_edge(selected_edge->actor, selected_edge->v_local_id);
        }
    }
}

