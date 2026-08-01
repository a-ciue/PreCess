#include "ModelOperator.h"
#include "GeometryData.h"
#include "ModelLayer.h"
#include "ModelObserver.h"

#include <spdlog/spdlog.h>

#include <stdexcept>
#include <utility>

ModelData& ModelOperator::data() const
{
    return *model_;
}

ModelObserver* ModelOperator::observer() const
{
    return observer_;
}

void ModelOperator::notifyChanged()
{
    if (this->observer_) {
        observer_->notifyModelChanged(this->id_);
    }
}

Index ModelOperator::addGeometryComponent(std::unique_ptr<ComponentData> component)
{
    if (!component || !component->geometry)
        throw std::invalid_argument("Geometry component must contain GeometryData");

    component->id = manager_->allocateComponentId();
    const Index component_id = component->id;
    spdlog::info("insert component: final_id={}, exists_before={}",
        component_id, manager_->components_.count(component_id) != 0);

    manager_->component_to_model_[component_id] = id_;
    model_->componentIds().push_back(component_id);
    manager_->components_[component_id] = std::move(component);
    manager_->components_[component_id]->geometry->ensureIndexBuilt(manager_->geom_registry_);

    // 这里只新增一个 Component，通知渲染层按组件加载，避免重载当前 Model。
    if (observer_)
        observer_->notifyComponentChanged(component_id);
    return component_id;
}

Index ModelOperator::getId() const
{
    return this->id_;
}
