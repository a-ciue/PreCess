#include "ModelOperator.h"
#include "GeometryData.h"
#include "ModelLayer.h"
#include "ModelObserver.h"
#include "UndoRecorder.h"

#include <spdlog/spdlog.h>

#include <stdexcept>
#include <utility>

ModelData& ModelOperator::data() const
{
    return *model_;
}

void ModelOperator::notifyChanged()
{
    if (manager_->observer_) {
        manager_->observer_->notifyModelChanged(this->id_);
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
    // 结构操作保持即时通知，不进待通知集合。
    if (manager_->observer_)
        manager_->observer_->notifyComponentChanged(component_id);
    // undo 记录钩子：组件加入后回调（结构操作即时成记录或并入当前操作边界）
    if (manager_->undo_recorder_)
        manager_->undo_recorder_->onComponentAdded(id_, component_id);
    return component_id;
}

Index ModelOperator::getId() const
{
    return this->id_;
}
