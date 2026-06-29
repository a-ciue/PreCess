#ifndef GEOMETRY_ACTOR_SELECT_OP_H
#define GEOMETRY_ACTOR_SELECT_OP_H
#include <memory>
#include <optional>
#include <vtkProp.h>

#include "GeometryActor.h"

class GeometryActorSelectOp;

class GeometryActorSelectOpFactory {
    friend GeometryActorSelectOp;

public:
    GeometryActorSelectOpFactory();
    GeometryActorSelectOpFactory(std::weak_ptr<const GeometryActor> geometry_actor);
    std::optional<GeometryActorSelectOp> lock();

private:
    std::weak_ptr<const GeometryActor> geometry_actor_;
};

class GeometryActorSelectOp {
    friend GeometryActorSelectOpFactory;

public:
    GeometryActorSelectOp(std::shared_ptr<const GeometryActor> geometry_actor);

    vtkProp& getPolyActor();
    vtkProp& getLineActor();
    const OccShapeHandle& getOccShape();
    const GeometrySubshapeIndex* getGeometryIndex();

private:
    std::shared_ptr<const GeometryActor> geometry_actor_;
};
#endif // GEOMETRY_ACTOR_SELECT_OP_H
