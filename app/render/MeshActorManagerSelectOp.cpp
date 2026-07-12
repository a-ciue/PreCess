#include "MeshActorManagerSelectOp.h"
#include "MeshActorManager.h"
#include "MeshActor.h"
#include <vtkHardwarePicker.h>

MeshActorManagerSelectOp::MeshActorManagerSelectOp(MeshActorManager& manager)
    : manager_(&manager)
{
}

std::optional<Index> MeshActorManagerSelectOp::getComponentId(vtkProp* prop) const
{
    auto it = prop_to_component_.find(prop);
    if (it != prop_to_component_.end()) return it->second;
    return std::nullopt;
}

void MeshActorManagerSelectOp::addPropsToPickList(vtkHardwarePicker* picker) const
{
    picker->PickFromListOn();
    for (const auto& [prop, _] : prop_to_component_) {
        picker->AddPickList(prop);
    }
}

std::optional<MeshActorSelectOp> MeshActorManagerSelectOp::getSelectOp(Index component_id) const
{
    auto actor = manager_->getComponentActor(component_id);
    if (!actor) return std::nullopt;
    return MeshActorSelectOp(actor);
}

void MeshActorManagerSelectOp::registerProps(Index component_id, std::shared_ptr<MeshActor> actor)
{
    MeshActorSelectOp op(actor);
    prop_to_component_[&op.getSolidActor()] = component_id;
    prop_to_component_[&op.getFaceActor()] = component_id;
    prop_to_component_[&op.getEdgeActor()] = component_id;
}

void MeshActorManagerSelectOp::unregisterProps(std::shared_ptr<MeshActor> actor)
{
    MeshActorSelectOp op(actor);
    prop_to_component_.erase(&op.getSolidActor());
    prop_to_component_.erase(&op.getFaceActor());
    prop_to_component_.erase(&op.getEdgeActor());
}
