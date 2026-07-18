#ifndef GEOMETRY_ACTOR_SELECT_OP_H
#define GEOMETRY_ACTOR_SELECT_OP_H
#include <memory>
#include <optional>
#include <vector>

#include <vtkSmartPointer.h>

#include "Core.h"
#include "GeometryActor.h"

class GeometryActorSelectOp;
class IVtkTools_ShapePicker;
class IVtkTools_SubPolyDataFilter;
class vtkActor;
class vtkRenderer;

class GeometryActorSelectOpFactory {
    friend GeometryActorSelectOp;

public:
    GeometryActorSelectOpFactory();
    GeometryActorSelectOpFactory(std::weak_ptr<GeometryActor> geometry_actor);
    std::optional<GeometryActorSelectOp> lock();

private:
    std::weak_ptr<GeometryActor> geometry_actor_;
};

class GeometryActorSelectOp {
    friend GeometryActorSelectOpFactory;

public:
    GeometryActorSelectOp(std::shared_ptr<GeometryActor> geometry_actor);

    static int toleranceForMode(SelectMode m);

    IVtk_IdType getShapeId() const;
    void disableSelectionModes(IVtkTools_ShapePicker* picker) const;
    void enableSelectionMode(IVtkTools_ShapePicker* picker, SelectMode mode) const;

    std::optional<Index> resolvePickedSubshape(IVtkTools_ShapePicker* picker, IVtk_IdType shapeId,
        SelectMode mode, IVtk_IdType& out_sub_id) const;

    bool resolvePickedSolid(IVtkTools_ShapePicker* picker, IVtk_IdType shapeId,
        GeomSolidId& out_solid_id, std::vector<IVtk_IdType>& out_face_sub_ids) const;

    vtkSmartPointer<IVtkTools_SubPolyDataFilter> buildHighlight(SelectMode mode);

    vtkActor& getPolyActor();
    bool isVisible() const;

private:
    std::shared_ptr<GeometryActor> geometry_actor_;
};
#endif // GEOMETRY_ACTOR_SELECT_OP_H
