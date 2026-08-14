#include "MeshActorManagerSelectOp.h"
#include "MeshActor.h"
#include "MeshActorManager.h"
#include "TopologyDiagnosticActor.h"
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

void MeshActorManagerSelectOp::registerProps(Index component_id, std::shared_ptr<MeshActor> actor)
{
    MeshActorSelectOp op(actor);
    prop_to_component_[&op.getSolidActor()] = component_id;
    prop_to_component_[&op.getFaceActor()] = component_id;
    prop_to_component_[&op.getEdgeActor()] = component_id;

    addToAllLists(&op.getSolidActor());
    addToAllLists(&op.getFaceActor());
    addToAllLists(&op.getEdgeActor());

    // 注册拓扑诊断 actor（非流形边、边界面等）
    auto& topo_diag = actor->topologyDiagnostics();
    for (size_t i = 0; i < static_cast<size_t>(TopologyDiagnosticCategory::Count); ++i) {
        auto category = static_cast<TopologyDiagnosticCategory>(i);
        if (auto* topo_actor = topo_diag.getActor(category)) {
            prop_to_component_[topo_actor] = component_id;
            addToAllLists(topo_actor);
        }
    }
}

void MeshActorManagerSelectOp::unregisterProps(std::shared_ptr<MeshActor> actor)
{
    MeshActorSelectOp op(actor);
    auto* solid = &op.getSolidActor();
    auto* face = &op.getFaceActor();
    auto* edge = &op.getEdgeActor();
    prop_to_component_.erase(solid);
    prop_to_component_.erase(face);
    prop_to_component_.erase(edge);

    // 注销拓扑诊断 actor
    std::set<vtkProp*> topo_actors;
    auto& topo_diag = actor->topologyDiagnostics();
    for (size_t i = 0; i < static_cast<size_t>(TopologyDiagnosticCategory::Count); ++i) {
        auto category = static_cast<TopologyDiagnosticCategory>(i);
        if (auto* topo_actor = topo_diag.getActor(category)) {
            prop_to_component_.erase(topo_actor);
            topo_actors.insert(topo_actor);
        }
    }

    removeFromAllLists({ solid, face, edge });
    if (!topo_actors.empty())
        removeFromAllLists(topo_actors);
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
