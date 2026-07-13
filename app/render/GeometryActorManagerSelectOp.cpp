#include "GeometryActorManagerSelectOp.h"
#include "GeometryActor.h"
#include "GeometryActorManager.h"
#include <vtkHardwarePicker.h>

GeometryActorManagerSelectOp::GeometryActorManagerSelectOp(GeometryActorManager& manager)
    : manager_(&manager)
{
}

std::optional<Index> GeometryActorManagerSelectOp::getComponentId(vtkProp* prop) const
{
    if (!prop) return std::nullopt;
    auto it = prop_to_component_.find(prop);
    if (it != prop_to_component_.end())
        return it->second;
    return std::nullopt;
}

void GeometryActorManagerSelectOp::managePickList(vtkPropCollection* pick_list)
{
    pick_list_ = pick_list;
}

std::optional<GeometryActorSelectOp> GeometryActorManagerSelectOp::getSelectOp(Index component_id) const
{
    auto actor = manager_->getComponentActor(component_id);
    if (!actor)
        return std::nullopt;
    return GeometryActorSelectOp(actor);
}

void GeometryActorManagerSelectOp::registerProps(Index component_id, std::shared_ptr<GeometryActor> actor)
{
    GeometryActorSelectOp op(actor);
    prop_to_component_[op.getPolyActor()] = component_id;

    if (pick_list_) {
        pick_list_->AddItem(op.getPolyActor());
    }
}

void GeometryActorManagerSelectOp::unregisterProps(std::shared_ptr<GeometryActor> actor)
{
    GeometryActorSelectOp op(actor);
    prop_to_component_.erase(op.getPolyActor());

    if (pick_list_) {
        pick_list_->RemoveItem(op.getPolyActor());
    }
}
