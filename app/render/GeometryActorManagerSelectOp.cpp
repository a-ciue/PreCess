#include "GeometryActorManagerSelectOp.h"
#include "GeometryActor.h"
#include "GeometryActorManager.h"
#include <IVtkTools_ShapePicker.hxx>

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

std::optional<Index> GeometryActorManagerSelectOp::getComponentIdByShapeId(IVtk_IdType shape_id) const
{
    auto it = shape_id_to_component_.find(shape_id);
    if (it != shape_id_to_component_.end())
        return it->second;
    return std::nullopt;
}

void GeometryActorManagerSelectOp::observeShapePicker(IVtkTools_ShapePicker* picker)
{
    if (!picker)
        return;
    shape_pickers_.push_back(picker);
    picker->PickFromListOn();
    picker->SetPixelTolerance(GeometryActorSelectOp::toleranceForMode(current_mode_));
    for (Index comp_id : registered_component_ids_) {
        auto actor = manager_->getComponentActor(comp_id);
        if (!actor)
            continue;
        GeometryActorSelectOp op(actor);
        picker->AddPickList(&op.getPolyActor());
        picker->AddPickList(&op.getLineActor());
        if (current_mode_ != SelectMode::None)
            op.enableSelectionMode(picker, current_mode_);
    }
}

void GeometryActorManagerSelectOp::unobserveShapePicker(IVtkTools_ShapePicker* picker)
{
    auto it = std::find(shape_pickers_.begin(), shape_pickers_.end(), picker);
    if (it != shape_pickers_.end())
        shape_pickers_.erase(it);
}

void GeometryActorManagerSelectOp::setShapePickerMode(SelectMode mode)
{
    current_mode_ = mode;
    for (auto picker : shape_pickers_) {
        picker->SetPixelTolerance(GeometryActorSelectOp::toleranceForMode(mode));
        for (Index comp_id : registered_component_ids_) {
            auto actor = manager_->getComponentActor(comp_id);
            if (!actor)
                continue;
            GeometryActorSelectOp op(actor);
            op.disableSelectionModes(picker);
            op.enableSelectionMode(picker, mode);
        }
    }
}

void GeometryActorManagerSelectOp::registerProps(Index component_id, std::shared_ptr<GeometryActor> actor)
{
    GeometryActorSelectOp op(actor);
    prop_to_component_[&op.getPolyActor()] = component_id;

    addToAllLists(&op.getPolyActor());

    registered_component_ids_.insert(component_id);
    shape_id_to_component_[op.getShapeId()] = component_id;

    addToAllShapePickers(&op.getPolyActor());
    addToAllShapePickers(&op.getLineActor());

    for (auto picker : shape_pickers_) {
        if (current_mode_ != SelectMode::None)
            op.enableSelectionMode(picker, current_mode_);
    }
}

void GeometryActorManagerSelectOp::unregisterProps(std::shared_ptr<GeometryActor> actor)
{
    GeometryActorSelectOp op(actor);
    auto* poly = &op.getPolyActor();
    auto* line = &op.getLineActor();
    prop_to_component_.erase(poly);

    removeFromAllLists(poly);

    auto shape_id = op.getShapeId();
    auto it = shape_id_to_component_.find(shape_id);
    if (it != shape_id_to_component_.end()) {
        registered_component_ids_.erase(it->second);
        shape_id_to_component_.erase(it);
    }

    removeFromAllShapePickers(poly);
    removeFromAllShapePickers(line);

    for (auto picker : shape_pickers_)
        op.disableSelectionModes(picker);
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

void GeometryActorManagerSelectOp::addToAllShapePickers(vtkProp* prop)
{
    for (auto picker : shape_pickers_)
        picker->AddPickList(prop);
}

void GeometryActorManagerSelectOp::removeFromAllShapePickers(vtkProp* prop)
{
    for (auto picker : shape_pickers_)
        picker->DeletePickList(prop);
}
