#ifndef MESH_ACTOR_MANAGER_SELECT_OP_H
#define MESH_ACTOR_MANAGER_SELECT_OP_H

#include "Core.h"
#include "MeshActorSelectOp.h"

#include <memory>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>
#include <vtkProp.h>

class MeshActorManager;
class MeshActor;
class vtkHardwarePicker;

class MeshActorManagerSelectOp {
public:
    explicit MeshActorManagerSelectOp(MeshActorManager& manager);

    std::optional<Index> getComponentId(vtkProp* prop) const;
    void observePickList(vtkPropCollection* pick_list);
    void unobservePickList(vtkPropCollection* pick_list);
    std::optional<MeshActorSelectOp> getSelectOp(Index component_id) const;

    void registerProps(Index component_id, std::shared_ptr<MeshActor> actor);
    void unregisterProps(std::shared_ptr<MeshActor> actor);

private:
    void addToAllLists(vtkProp* prop);
    void removeFromAllLists(const std::set<vtkProp*>& props);

    MeshActorManager* manager_ { };
    std::unordered_map<vtkProp*, Index> prop_to_component_;
    std::vector<vtkSmartPointer<vtkPropCollection>> pick_lists_;
};

#endif
