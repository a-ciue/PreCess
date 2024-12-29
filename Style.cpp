#include "Style.h"
#include <vtkRenderWindowInteractor.h>
#include "ModelActor.h"
#include "MyVtkItem.h"

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

std::vector<int> MouseInteractorHighLightActor::GetSelectedIDs(ModelActor* mActor, SelectMode mode)
{
    auto actors = selector_->get();
    std::vector<int> ids;

    int (ModelActor::*actor_id)(vtkActor*) {};
    if (mode == SelectMode::Group)
    {
        actor_id = &ModelActor::group_actor_id;
    } else if (mode == SelectMode::Block) {
        actor_id = &ModelActor::block_actor_id;
    } else {
        assert(false);
    }

    for (vtkActor* actor : actors)
    {
        ids.push_back((mActor->*actor_id)(actor));
    }
    return ids;
}
std::vector<int> MouseInteractorHighLightFace::GetSelectedIDs(ModelActor* mActor, SelectMode mode)
{
    auto actors = selector_->get();
    std::vector<int> face_ids;
    if (actors)
    {
        face_ids.reserve(2);

        int patch_id = mActor->patch_actor_id(actors->patch_actor);
        face_ids.push_back(patch_id);
        face_ids.push_back(mActor->patch_global_fid(patch_id,actors->local_id));
    }
    
    return face_ids;
}
std::vector<int> MouseInteractorHighLightEdge::GetSelectedIDs(ModelActor* mActor, SelectMode mode)
{
    auto actors = selector_->get();
    std::vector<int> point_ids;

    if (actors)
    {
        point_ids.reserve(3);

        int patch_id = mActor->patch_actor_id(actors->actor);
        point_ids.push_back(patch_id);
        point_ids.push_back(mActor->patch_global_vid(patch_id,actors->v_local_id[0]));
        point_ids.push_back(mActor->patch_global_vid(patch_id, actors->v_local_id[1]));
    }

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


