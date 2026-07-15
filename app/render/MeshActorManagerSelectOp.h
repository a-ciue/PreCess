#ifndef MESH_ACTOR_MANAGER_SELECT_OP_H
#define MESH_ACTOR_MANAGER_SELECT_OP_H

#include "Core.h"
#include "MeshActorSelectOp.h"

#include <memory>
#include <optional>
#include <unordered_map>
#include <vtkProp.h>

class MeshActorManager;
class MeshActor;
class vtkHardwarePicker;

class MeshActorManagerSelectOp {
public:
    explicit MeshActorManagerSelectOp(MeshActorManager& manager);

    std::optional<Index> getComponentId(vtkProp* prop) const;
    void managePickList(vtkPropCollection* pick_list);
    std::optional<MeshActorSelectOp> getSelectOp(Index component_id) const;

    void registerProps(Index component_id, std::shared_ptr<MeshActor> actor);
    void unregisterProps(std::shared_ptr<MeshActor> actor);

private:
    MeshActorManager* manager_ { };
    std::unordered_map<vtkProp*, Index> prop_to_component_;
    vtkSmartPointer<vtkPropCollection> pick_list_;
};

#endif
