#include "MeshActorSelectOp.h"
#include "MeshActor.h"

MeshActorSelectOp::MeshActorSelectOp() = default;

MeshActorSelectOp::MeshActorSelectOp(std::weak_ptr<const MeshActor> mesh_actor)
    : mesh_actor_(mesh_actor)
{
}

bool MeshActorSelectOp::addPickList(vtkPropCollection* pick_list)
{
    if (auto mesh_actor = mesh_actor_.lock()) {
        pick_list->AddItem(mesh_actor->actor_);
        pick_list->AddItem(mesh_actor->edge_actor_);
        pick_list->AddItem(mesh_actor->face_actor_);
        pick_list->AddItem(mesh_actor->solid_actor_);
        return true;
    }
    return false;
}

Index MeshActorSelectOp::getModelBlockId(vtkIdType block_id)
{
    if (auto mesh_actor = mesh_actor_.lock()) {
        return mesh_actor->model_data_->model_block_id(block_id);
    }
    return -1;
}

vtkIdType MeshActorSelectOp::getSolidIdByFace(vtkIdType face_id)
{
    if (auto mesh_actor = mesh_actor_.lock()) {
        //mesh_actor->solid_data_
    }
    return -1;
}