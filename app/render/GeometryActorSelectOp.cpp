#include "GeometryActorSelectOp.h"
#include "GeometryActor.h"

GeometryActorSelectOpFactory::GeometryActorSelectOpFactory() = default;
GeometryActorSelectOpFactory::GeometryActorSelectOpFactory(std::weak_ptr<const GeometryActor> geometry_actor)
    : geometry_actor_(geometry_actor)
{
}

std::optional<GeometryActorSelectOp> GeometryActorSelectOpFactory::lock()
{
    if (auto geometry_actor = geometry_actor_.lock()) {
        return { geometry_actor };
    }
    return {};
}

GeometryActorSelectOp::GeometryActorSelectOp(std::shared_ptr<const GeometryActor> geometry_actor)
    : geometry_actor_(geometry_actor)
{
    if (!geometry_actor_) {
        throw std::runtime_error("GeometryActorSelectOp: geometry_actor is nullptr");
    }
}

vtkProp& GeometryActorSelectOp::getPolyActor()
{
    return *const_cast<vtkActor*>(geometry_actor_->polyActor());
}

vtkProp& GeometryActorSelectOp::getLineActor()
{
    return *const_cast<vtkActor*>(geometry_actor_->lineActor());
}

const OccShapeHandle& GeometryActorSelectOp::getOccShape()
{
    return geometry_actor_->getOccShape();
}
