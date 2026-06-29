#include "QRenderWindowStyle.h"
#include "SelectManager.h"
#include "GeometrySelectManager.h"
#include <vtkObjectFactory.h>
#include <vtkRenderWindowInteractor.h>

vtkStandardNewMacro(QRenderWindowStyle);

void QRenderWindowStyle::SetClick()
{
	click_ = true;
}

void QRenderWindowStyle::SetSelectManager(SelectManager* select_manager)
{
	this->select_manager_ = select_manager;
}

void QRenderWindowStyle::SetGeometrySelectManager(GeometrySelectManager* geometry_select_manager)
{
	this->geometry_select_manager_ = geometry_select_manager;
}

void QRenderWindowStyle::OnLeftButtonUp()
{
	if (click_) {
		click_ = false;
		int pos[2];
		this->GetInteractor()->GetEventPosition(pos);
		if (select_manager_)
			select_manager_->select(pos[0], pos[1]);
		if (geometry_select_manager_)
			geometry_select_manager_->select(pos[0], pos[1]);
	}
		
	vtkInteractorStyleTrackballCamera::OnLeftButtonUp();
}

//vtkStandardNewMacro(MouseInteractorHighLightActor);
//void MouseInteractorHighLightActor::OnLeftButtonUp()
//{
//    if (selector_ != nullptr && click_)
//    {
//        click_ = false;
//        int pos[2];
//        this->GetInteractor()->GetEventPosition(pos);
//        OnSelect(pos[0], pos[1]);
//    }
//    vtkInteractorStyleTrackballCamera::OnLeftButtonUp();
//}
//
//void MouseInteractorHighLightActor::OnSelect(double posx, double posy)
//{
//    selector_->select(posx, posy);
//}
//
//
//
//void MouseInteractorHighLightActor::SetClick()
//{
//    click_ = true;
//}
//
//void MouseInteractorHighLightActor::ClearSelections()
//{
//    selector_->clear();
//}
//
//std::unique_ptr<Selection> MouseInteractorHighLightActor::GetSelectedIDs(const std::unordered_map<QString, std::unique_ptr<MeshActor>>& mActors, SelectMode mode)
//{
//    auto actors = selector_->get();
//    std::unique_ptr<Selection> selection = std::make_unique<Selection>();
//    //std::vector<int> ids;
//    vtkPropAssembly* assembly_ = selector_->getAssembly();
//    MeshActor* modelactor_;
//
//    for (const auto& pair : mActors)
//    {
//        modelactor_= pair.second.get()->getModelActor(assembly_);
//
//        if (modelactor_ != NULL)
//        {
//            selection->model_name = pair.first;
//            break;
//        }
//    }
//
//
//    int (MeshActor::*actor_id)(vtkActor*) {};
//    if (mode == SelectMode::Group)
//    {
//        actor_id = &MeshActor::group_actor_id;
//        selection->type = Element::Type::Group;
//    } else if (mode == SelectMode::Block) {
//        actor_id = &MeshActor::block_actor_id;
//        selection->type = Element::Type::Block;
//    } else {
//        assert(false);
//    }
//
//    for (vtkActor* actor : actors)
//    {
//        selection->ids.push_back((modelactor_->*actor_id)(actor));
//    }
//    return selection;
//}
//std::unique_ptr<Selection> MouseInteractorHighLightFace::GetSelectedIDs(const std::unordered_map<QString, std::unique_ptr<MeshActor>>& mActors, SelectMode mode)
//{
//    auto actors = selector_->get();
//    std::unique_ptr<Selection> selection = std::make_unique<Selection>();
//    //std::vector<int> face_ids;
//    vtkPropAssembly* assembly_ = selector_->getAssembly();
//    MeshActor* modelactor_;
//    for (const auto& pair : mActors)
//    {
//        modelactor_ = pair.second.get()->getModelActor(assembly_);
//
//        if (modelactor_ != NULL)
//        {
//            selection->model_name = pair.first;
//            break;
//        }
//    }
//    if (actors)
//    {
//        selection->ids.reserve(2);
//
//        int patch_id = modelactor_->patch_actor_id(actors->patch_actor);
//        selection->ids.push_back(modelactor_->patch_global_fid(patch_id,actors->local_id));
//        selection->type = Element::Type::Face;
//    }
//    
//    return selection;
//}
//std::unique_ptr<Selection> MouseInteractorHighLightEdge::GetSelectedIDs(const std::unordered_map<QString, std::unique_ptr<MeshActor>>& mActors, SelectMode mode)
//{
//    auto actors = selector_->get();
//    std::unique_ptr<Selection> selection=std::make_unique<Selection>();
//    //std::vector<int> point_ids;
//    vtkPropAssembly* assembly_ = selector_->getAssembly();
//    MeshActor* modelactor_;
//    for (const auto& pair : mActors)
//    {
//        modelactor_ = pair.second.get()->getModelActor(assembly_);
//
//        if (modelactor_ != NULL)
//        {
//            selection->model_name = pair.first;
//            break;
//        }
//    }
//    if (actors)
//    {
//        selection->ids.reserve(3);
//
//        int patch_id = modelactor_->patch_actor_id(actors->actor);
//        selection->ids.push_back(modelactor_->patch_global_vid(patch_id,actors->v_local_id[0]));
//        selection->ids.push_back(modelactor_->patch_global_vid(patch_id, actors->v_local_id[1]));
//        selection->type = Element::Type::Edge;
//    }
//    
//    return selection;
//}
//
//void MouseInteractorHighLightActor::SetSelector(std::unique_ptr<BlockSelectorHighlight> selector)
//{
//    selector_ = std::move(selector);
//}
//
//
//vtkStandardNewMacro(MouseInteractorHighLightFace);
//void MouseInteractorHighLightFace::OnLeftButtonUp()
//{
//    if (selector_ != nullptr && click_)
//    {
//        click_ = false;
//        int pos[2];
//        this->GetInteractor()->GetEventPosition(pos);
//        selector_->select(pos[0], pos[1]);
//    }
//    vtkInteractorStyleTrackballCamera::OnLeftButtonUp();
//}
//
//
//
//void MouseInteractorHighLightFace::SetClick()
//{
//    click_ = true;
//}
//
//void MouseInteractorHighLightFace::ClearSelections()
//{
//    selector_->clear();
//}
//
//void MouseInteractorHighLightFace::SetSelector(std::unique_ptr<SingleFaceSelectorHighlight> selector)
//{
//    selector_ = std::move(selector);
//}
//
//vtkStandardNewMacro(MouseInteractorHighLightEdge);
//void MouseInteractorHighLightEdge::OnLeftButtonUp()
//{
//    if (selector_ != nullptr && click_)
//    {
//        click_ = false;
//        int pos[2];
//        this->GetInteractor()->GetEventPosition(pos);
//        selector_->select(pos[0], pos[1]);
//    }
//    vtkInteractorStyleTrackballCamera::OnLeftButtonUp();
//}
//
//
//void MouseInteractorHighLightEdge::SetClick()
//{
//    click_ = true;
//}
//
//void MouseInteractorHighLightEdge::ClearSelections()
//{
//    selector_->clear();
//}
//
//
//void MouseInteractorHighLightEdge::SetSelector(std::unique_ptr<SingleEdgeSelectorHighlight> selector)
//{
//    selector_ = std::move(selector);
//}


