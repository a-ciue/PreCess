#include "GeometryActorSelectOp.h"
#include "GeometryActor.h"
#include <vtkActor.h>
#include <IVtkTools_SubPolyDataFilter.hxx>

GeometryActorSelectOpFactory::GeometryActorSelectOpFactory() = default;
GeometryActorSelectOpFactory::GeometryActorSelectOpFactory(std::weak_ptr<GeometryActor> geometry_actor)
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

GeometryActorSelectOp::GeometryActorSelectOp(std::shared_ptr<GeometryActor> geometry_actor)
    : geometry_actor_(geometry_actor)
{
    if (!geometry_actor_) {
        throw std::runtime_error("GeometryActorSelectOp: geometry_actor is nullptr");
    }
}

vtkProp& GeometryActorSelectOp::getPolyActor()
{
    return *geometry_actor_->polyActor();
}

vtkProp& GeometryActorSelectOp::getLineActor()
{
    return *geometry_actor_->lineActor();
}

vtkActor& GeometryActorSelectOp::getPolyHLActor()
{
    return *geometry_actor_->polyHLActor();
}

vtkActor& GeometryActorSelectOp::getLineHLActor()
{
    return *geometry_actor_->lineHLActor();
}

IVtkTools_SubPolyDataFilter& GeometryActorSelectOp::getPolyHLFilter()
{
    return *geometry_actor_->polyHLFilter();
}

IVtkTools_SubPolyDataFilter& GeometryActorSelectOp::getLineHLFilter()
{
    return *geometry_actor_->lineHLFilter();
}

const OccShapeHandle& GeometryActorSelectOp::getOccShape()
{
    return geometry_actor_->getOccShape();
}

const GeometrySubshapeIndex* GeometryActorSelectOp::getGeometryIndex()
{
    return geometry_actor_->geometryIndex();
}

const vtkDataArray* GeometryActorSelectOp::getLineSubIdArray() const
{
    return geometry_actor_->lineSubIdArray();
}

const vtkDataArray* GeometryActorSelectOp::getPolySubIdArray() const
{
    return geometry_actor_->polySubIdArray();
}
