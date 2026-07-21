#include "ComponentOperator.h"

#include "ComponentData.h"
#include "ModelLayer.h"
#include "ModelObserver.h"
#include "MeshData.h"
#include "GeometryData.h"
#include "ModelData.h"

#include <stdexcept>

ComponentOperator::ComponentOperator(Index component_id,
    ComponentData& component,
    ModelLayer& mgr,
    ModelObserver* observer,
    Index model_id) noexcept
    : component_id_(component_id)
    , model_id_(model_id)
    , component_(&component)
    , mgr_(&mgr)
    , observer_(observer)
{
}

MeshData* ComponentOperator::mesh() const noexcept
{
    return component_ && component_->mesh ? component_->mesh.get() : nullptr;
}

GeometryData* ComponentOperator::geometry() const noexcept
{
    return component_ && component_->geometry ? component_->geometry.get() : nullptr;
}

Index ComponentOperator::modelId() const noexcept
{
    return model_id_;
}

ModelData* ComponentOperator::model() const
{
    return mgr_->modelById(model_id_);
}

void ComponentOperator::notifyChanged() const
{
    if (!observer_) return;

    observer_->notifyComponentChanged(component_id_);
}

void ComponentOperator::removeMesh()
{
    if (!component_ || !component_->mesh)
        return;

    component_->mesh->releaseEdgeIdMap(mgr_->edgeIdMap());
    component_->mesh.reset();

    if (observer_)
        observer_->notifyComponentChanged(component_id_);
}

void ComponentOperator::removeGeometry()
{
    if (!component_ || !component_->geometry)
        return;

    component_->geometry->index.release(mgr_->geomRegistry());
    component_->geometry.reset();

    if (observer_)
        observer_->notifyComponentChanged(component_id_);
}