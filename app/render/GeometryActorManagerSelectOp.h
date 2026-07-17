#ifndef GEOMETRY_ACTOR_MANAGER_SELECT_OP_H
#define GEOMETRY_ACTOR_MANAGER_SELECT_OP_H

#include "Core.h"
#include "GeometryActorSelectOp.h"

#include <memory>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>
#include <vtkProp.h>

class GeometryActorManager;
class GeometryActor;
class IVtkTools_ShapePicker;

class GeometryActorManagerSelectOp {
public:
    explicit GeometryActorManagerSelectOp(GeometryActorManager& manager);

    std::optional<Index> getComponentId(vtkProp* prop) const;
    void observePickList(vtkPropCollection* pick_list);
    void unobservePickList(vtkPropCollection* pick_list);

    std::optional<Index> getComponentIdByShapeId(IVtk_IdType shape_id) const;
    void observeShapePicker(IVtkTools_ShapePicker* picker);
    void unobserveShapePicker(IVtkTools_ShapePicker* picker);
    void setShapePickerMode(SelectMode mode);

    std::optional<GeometryActorSelectOp> getSelectOp(Index component_id) const;

    void registerProps(Index component_id, std::shared_ptr<GeometryActor> actor);
    void unregisterProps(std::shared_ptr<GeometryActor> actor);

private:
    void addToAllLists(vtkProp* prop);
    void removeFromAllLists(vtkProp* prop);
    void addToAllShapePickers(vtkProp* prop);
    void removeFromAllShapePickers(vtkProp* prop);

    GeometryActorManager* manager_ { };
    std::unordered_map<vtkProp*, Index> prop_to_component_;
    std::vector<vtkSmartPointer<vtkPropCollection>> pick_lists_;

    std::unordered_map<IVtk_IdType, Index> shape_id_to_component_;
    std::set<Index> registered_component_ids_;
    std::vector<vtkSmartPointer<IVtkTools_ShapePicker>> shape_pickers_;
    SelectMode current_mode_ { SelectMode::None };
};

#endif
