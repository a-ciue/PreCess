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
    if (!prop)
        return std::nullopt;
    auto it = prop_to_component_.find(prop);
    if (it != prop_to_component_.end())
        return it->second;
    return std::nullopt;
}

void GeometryActorManagerSelectOp::observePickList(vtkPropCollection* pick_list)
{
    if (pick_list)
        pick_lists_.push_back(pick_list);
}

void GeometryActorManagerSelectOp::unobservePickList(vtkPropCollection* pick_list)
{
    auto it = std::find(pick_lists_.begin(), pick_lists_.end(), pick_list);
    if (it != pick_lists_.end())
        pick_lists_.erase(it);
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
    prop_to_component_[&op.getPolyActor()] = component_id;

    addToAllLists(&op.getPolyActor());
}

void GeometryActorManagerSelectOp::unregisterProps(std::shared_ptr<GeometryActor> actor)
{
    GeometryActorSelectOp op(actor);
    auto* poly = &op.getPolyActor();
    prop_to_component_.erase(poly);

    removeFromAllLists(poly);
}

void GeometryActorManagerSelectOp::addToAllLists(vtkProp* prop)
{
    for (auto& list : pick_lists_)
        list->AddItem(prop);
}

void GeometryActorManagerSelectOp::removeFromAllLists(vtkProp* prop)
{
    for (auto& list : pick_lists_)
        list->RemoveItem(prop);
}
