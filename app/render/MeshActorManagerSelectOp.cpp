#include "MeshActorManagerSelectOp.h"
#include "MeshActor.h"
#include "MeshActorManager.h"
#include <set>
#include <vtkHardwarePicker.h>

MeshActorManagerSelectOp::MeshActorManagerSelectOp(MeshActorManager& manager)
    : manager_(&manager)
{
}

std::optional<Index> MeshActorManagerSelectOp::getComponentId(vtkProp* prop) const
{
    if (!prop)
        return std::nullopt;
    auto it = prop_to_component_.find(prop);
    if (it != prop_to_component_.end())
        return it->second;
    return std::nullopt;
}

void MeshActorManagerSelectOp::observePickList(vtkPropCollection* pick_list)
{
    if (pick_list)
        pick_lists_.push_back(pick_list);
}

void MeshActorManagerSelectOp::unobservePickList(vtkPropCollection* pick_list)
{
    auto it = std::find(pick_lists_.begin(), pick_lists_.end(), pick_list);
    if (it != pick_lists_.end())
        pick_lists_.erase(it);
}

std::optional<MeshActorSelectOp> MeshActorManagerSelectOp::getSelectOp(Index component_id) const
{
    auto actor = manager_->getComponentActor(component_id);
    if (!actor)
        return std::nullopt;
    return MeshActorSelectOp(actor);
}

std::vector<Index> MeshActorManagerSelectOp::getAllComponentIds() const
{
    // 由 registerProps/unregisterProps 维护的已注册组件集合
    return { registered_component_ids_.begin(), registered_component_ids_.end() };
}

void MeshActorManagerSelectOp::registerProps(Index component_id, std::shared_ptr<MeshActor> actor)
{
    MeshActorSelectOp op(actor);
    prop_to_component_[&op.getSolidActor()] = component_id;
    prop_to_component_[&op.getFaceActor()] = component_id;
    prop_to_component_[&op.getEdgeActor()] = component_id;
    registered_component_ids_.insert(component_id);

    addToAllLists(&op.getSolidActor());
    addToAllLists(&op.getFaceActor());
    addToAllLists(&op.getEdgeActor());
}

void MeshActorManagerSelectOp::unregisterProps(std::shared_ptr<MeshActor> actor)
{
    MeshActorSelectOp op(actor);
    auto* solid = &op.getSolidActor();
    auto* face = &op.getFaceActor();
    auto* edge = &op.getEdgeActor();

    auto comp_it = prop_to_component_.find(solid);
    if (comp_it != prop_to_component_.end())
        registered_component_ids_.erase(comp_it->second);
    prop_to_component_.erase(solid);
    prop_to_component_.erase(face);
    prop_to_component_.erase(edge);

    removeFromAllLists({ solid, face, edge });
}

void MeshActorManagerSelectOp::addToAllLists(vtkProp* prop)
{
    for (auto& list : pick_lists_)
        list->AddItem(prop);
}

void MeshActorManagerSelectOp::removeFromAllLists(const std::set<vtkProp*>& props)
{
    for (auto& list : pick_lists_) {
        list->InitTraversal();
        vtkProp* prop;
        while ((prop = list->GetNextProp()) != nullptr) {
            if (props.count(prop)) {
                list->RemoveItem(prop);
            }
        }
    }
}
