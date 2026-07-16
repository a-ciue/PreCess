#ifndef GEOMETRY_ACTOR_MANAGER_SELECT_OP_H
#define GEOMETRY_ACTOR_MANAGER_SELECT_OP_H

#include "Core.h"
#include "GeometryActorSelectOp.h"

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>
#include <vtkProp.h>

class GeometryActorManager;
class GeometryActor;
class vtkHardwarePicker;

class GeometryActorManagerSelectOp {
public:
    explicit GeometryActorManagerSelectOp(GeometryActorManager& manager);

    std::optional<Index> getComponentId(vtkProp* prop) const;
    void observePickList(vtkPropCollection* pick_list);
    void unobservePickList(vtkPropCollection* pick_list);
    std::optional<GeometryActorSelectOp> getSelectOp(Index component_id) const;

    void registerProps(Index component_id, std::shared_ptr<GeometryActor> actor);
    void unregisterProps(std::shared_ptr<GeometryActor> actor);

private:
    void addToAllLists(vtkProp* prop);
    void removeFromAllLists(vtkProp* prop);

    GeometryActorManager* manager_ { };
    std::unordered_map<vtkProp*, Index> prop_to_component_;
    std::vector<vtkSmartPointer<vtkPropCollection>> pick_lists_;
};

#endif
