#ifndef GEOMETRY_ACTOR_SELECT_OP_H
#define GEOMETRY_ACTOR_SELECT_OP_H
#include <memory>
#include <optional>
#include <vtkProp.h>

#include <Standard_Handle.hxx>
#include <IVtkOCC_Shape.hxx>
typedef Handle(IVtkOCC_Shape) OccShapeHandle;

class GeometryActorSelectOp;
class GeometryActor;

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

private:
    std::shared_ptr<const GeometryActor> geometry_actor_;
};
#endif // GEOMETRY_ACTOR_SELECT_OP_H
