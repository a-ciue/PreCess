#ifndef GEOMETRY_ACTOR_SELECT_OP_H
#define GEOMETRY_ACTOR_SELECT_OP_H
#include <memory>
#include <optional>
#include <vtkDataArray.h>
#include <vtkProp.h>

#include "GeometryActor.h"

class GeometryActorSelectOp;
class IVtkTools_SubPolyDataFilter;
class vtkActor;

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

    vtkProp& getPolyActor();
    vtkProp& getLineActor();
    vtkActor& getPolyHLActor();
    vtkActor& getLineHLActor();
    IVtkTools_SubPolyDataFilter& getPolyHLFilter();
    IVtkTools_SubPolyDataFilter& getLineHLFilter();
    const OccShapeHandle& getOccShape();
    const GeometrySubshapeIndex* getGeometryIndex();
    const vtkDataArray* getLineSubIdArray() const;
    const vtkDataArray* getPolySubIdArray() const;

private:
    std::shared_ptr<GeometryActor> geometry_actor_;
};
#endif // GEOMETRY_ACTOR_SELECT_OP_H
