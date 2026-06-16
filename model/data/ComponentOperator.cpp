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
                                     ModelObserver* observer) noexcept
    : component_id_(component_id)
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

Index ComponentOperator::modelId() const
{
    auto mid = mgr_->findModelIdByComponent(component_id_);
    if (!mid)
        throw std::runtime_error("ComponentOperator::modelId: owner model not found");
    return *mid;
}

ModelData* ComponentOperator::model() const
{
    auto mid = mgr_->findModelIdByComponent(component_id_);
    if (!mid) return nullptr;
    return mgr_->modelById(*mid); // 见第2步：给 ModelLayer 加 public 访问器
}

void ComponentOperator::notifyChanged() const
{
    if (!observer_) return;

    observer_->notifyComponentChanged(component_id_);
}