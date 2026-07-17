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

void GeometryActorManagerSelectOp::observePickList(vtkSmartPointer<vtkPropCollection> pick_list)
{
    if (pick_list)
        pick_lists_.push_back(pick_list);
}

void GeometryActorManagerSelectOp::unobservePickList(vtkSmartPointer<vtkPropCollection> pick_list)
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

void GeometryActorManagerSelectOp::observeShapePicker(vtkSmartPointer<IVtkTools_ShapePicker> picker)
{
    if (!picker)
        return;
    shape_pickers_.push_back(picker);
    picker->SetPixelTolerance(GeometryActorSelectOp::toleranceForMode(current_mode_));
    for (Index comp_id : registered_component_ids_) {
        auto actor = manager_->getComponentActor(comp_id);
        if (!actor)
            continue;
        GeometryActorSelectOp op(actor);
        if (current_mode_ != SelectMode::None && actor->isVisible())
            op.enableSelectionMode(picker, current_mode_);
    }
}

void GeometryActorManagerSelectOp::unobserveShapePicker(vtkSmartPointer<IVtkTools_ShapePicker> picker)
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
            if (actor->isVisible())
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

    for (auto picker : shape_pickers_) {
        if (current_mode_ != SelectMode::None && actor->isVisible())
            op.enableSelectionMode(picker, current_mode_);
    }
}

void GeometryActorManagerSelectOp::unregisterProps(std::shared_ptr<GeometryActor> actor)
{
    GeometryActorSelectOp op(actor);
    auto* poly = &op.getPolyActor();
    prop_to_component_.erase(poly);

    removeFromAllLists(poly);

    auto shape_id = op.getShapeId();
    auto it = shape_id_to_component_.find(shape_id);
    if (it != shape_id_to_component_.end()) {
        registered_component_ids_.erase(it->second);
        shape_id_to_component_.erase(it);
    }

    for (auto picker : shape_pickers_)
        op.disableSelectionModes(picker);
}

void GeometryActorManagerSelectOp::setShapePickingEnabled(std::shared_ptr<GeometryActor> actor, bool enabled)
{
    GeometryActorSelectOp op(actor);
    for (auto picker : shape_pickers_) {
        if (enabled) {
            if (current_mode_ != SelectMode::None)
                op.enableSelectionMode(picker, current_mode_);
        } else {
            op.disableSelectionModes(picker);
        }
    }
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
