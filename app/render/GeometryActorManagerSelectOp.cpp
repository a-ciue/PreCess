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
    auto it = prop_to_component_.find(prop);
    if (it != prop_to_component_.end())
        return it->second;
    return std::nullopt;
}

void GeometryActorManagerSelectOp::addPropsToPickList(vtkHardwarePicker* picker) const
{
    picker->PickFromListOn();
    for (const auto& [prop, _] : prop_to_component_) {
        picker->AddPickList(prop);
    }
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
    prop_to_component_[op.getLineActor()] = component_id;
}

void GeometryActorManagerSelectOp::unregisterProps(std::shared_ptr<GeometryActor> actor)
{
    GeometryActorSelectOp op(actor);
    prop_to_component_.erase(op.getPolyActor());
    prop_to_component_.erase(op.getLineActor());
}
