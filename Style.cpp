#include "Style.h"
#include <vtkRenderWindowInteractor.h>
#include "ModelActor.h"

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



void MouseInteractorHighLightActor::SetClick()
{
    click_ = true;
}

void MouseInteractorHighLightActor::ClearSelections()
{
    selector_->clear();
}

std::vector<int> MouseInteractorHighLightActor::GetSelectedIDs(ModelActor* mActor)
{
    auto actors = selector_->get();
    std::vector<int> block_ids;
    for (vtkActor* actor : actors)
    {
        block_ids.push_back(mActor->block_actor_id(actor));
    }
    return block_ids;
}
std::vector<int> MouseInteractorHighLightFace::GetSelectedIDs(ModelActor* mActor)
{
    auto actors = selector_->get();
    std::vector<int> face_ids;
    int paches_id = mActor->block_actor_id(actors->patch_actor);
    face_ids.push_back(mActor->patch_global_fid(paches_id,actors->local_id));
    
    return face_ids;
}
std::vector<int> MouseInteractorHighLightEdge::GetSelectedIDs(ModelActor* mActor)
{
    auto actors = selector_->get();
    std::vector<int> point_ids;
    int paches_id = mActor->block_actor_id(actors->actor);
    point_ids.push_back(mActor->patch_global_vid(paches_id,actors->v_local_id[0]));
    point_ids.push_back(mActor->patch_global_vid(paches_id, actors->v_local_id[1]));

    return point_ids;
}

void MouseInteractorHighLightActor::SetSelector(std::unique_ptr<ActorSelectorHighlight> selector)
{
    selector_ = std::move(selector);
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


