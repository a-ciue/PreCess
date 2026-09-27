#include "GeometryActorManager.h"
#include "GeometryActor.h"
#include "GeometryInterferenceController.h"
#include "GeometryTopologyDiagnosticActor.h"

#include "Core.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>
#include <spdlog/spdlog.h>

namespace {
/** @brief 生成日志使用的“id (名称)”标签，名称为空时只保留 id。 */
std::string displayLabel(Index id, const std::string& name)
{
    return std::to_string(id) + (name.empty() ? std::string() : " (" + name + ")");
}
}

GeometryActorManager::GeometryActorManager()
    : interference_(std::make_unique<GeometryInterferenceController>(*this))
{
}

GeometryActorManager::~GeometryActorManager() = default;

void GeometryActorManager::bindRender(vtkRenderer* renderer)
{
    this->renderer_ = renderer;
}

std::shared_ptr<GeometryActor> GeometryActorManager::getComponentActor(Index component_id) const
{
    auto it = component_actors_.find(component_id);
    if (it != component_actors_.end()) {
        return it->second;
    }
    return nullptr;
}

bool GeometryActorManager::hasComponent(Index component_id) const
{
    return component_actors_.count(component_id) != 0;
}

void GeometryActorManager::deleteComponent(Index component_id)
{
    const std::optional<Index> model_id = interference_->untrackComponent(component_id);
    auto it = component_actors_.find(component_id);
    if (it != component_actors_.end()) {
        op_.unregisterProps(it->second);
        component_actors_.erase(it);
    }
    if (model_id.has_value())
        interference_->modelChanged(*model_id);
}

void GeometryActorManager::deleteModel(Index model_id)
{
    // 删除 Model 时批量清 Actor，避免每删一个 Component 都重算一次干涉。
    for (Index component_id : interference_->componentIds(model_id)) {
        auto it = component_actors_.find(component_id);
        if (it != component_actors_.end()) {
            op_.unregisterProps(it->second);
            component_actors_.erase(it);
        }
    }
    interference_->removeModel(model_id);
}

void GeometryActorManager::loadGeometry(const GeometryDataVtk& geometry_data)
{
    Index component_id = geometry_data.component_id;

    auto actor_it = component_actors_.find(component_id);
    if (actor_it == component_actors_.end()) {
        component_actors_[component_id] = std::make_shared<GeometryActor>(this->renderer_);
    } else {
        // 先用旧 OCC Shape 注销 Picker 状态，再由 loadShape 替换几何数据。
        op_.unregisterProps(actor_it->second);
    }

    auto& actor_ptr = component_actors_[component_id];
    // 先同步类别开关但不计算，避免复用 Actor 时对即将替换的旧 Shape 重建诊断。
    actor_ptr->topologyDiagnostics().setCategoryFlagsOnly(
        topology_diagnostic_category_enabled_);
    actor_ptr->topologyDiagnostics().setSmallEdgeLengthThreshold(
        topology_diagnostic_small_edge_length_);
    actor_ptr->topologyDiagnostics().setSmallFaceAreaThreshold(
        topology_diagnostic_small_face_area_);
    actor_ptr->loadShape(geometry_data);
    actor_ptr->setRenderStyle(current_style_);
    op_.registerProps(component_id, actor_ptr);
}

void GeometryActorManager::loadGeometry(const GeometryDataVtk& geometry_data, Index model_id,
    const std::string& model_name, const std::string& component_name)
{
    loadGeometry(geometry_data);
    interference_->trackComponent(model_id, geometry_data.component_id,
        geometry_data.shape, displayLabel(model_id, model_name));
    component_actors_[geometry_data.component_id]->topologyDiagnostics().setComponentLabel(
        displayLabel(geometry_data.component_id, component_name));
}

void GeometryActorManager::modelChanged(Index model_id)
{
    interference_->modelChanged(model_id);
}

void GeometryActorManager::setVisibility(Index component_id, bool visibility)
{
    auto it = component_actors_.find(component_id);
    if (it != component_actors_.end()) {
        it->second->setVisibility(visibility);
        op_.setShapePickingEnabled(it->second, visibility);
    }
}

void GeometryActorManager::setCurrentRenderStyle(GeometryRenderStyle style)
{
    current_style_ = style;
    for (auto& [id, actor] : component_actors_) {
        actor->setRenderStyle(style);
    }
}

GeometryRenderStyle GeometryActorManager::getCurrentRenderStyle() const
{
    return current_style_;
}

void GeometryActorManager::setTopologyDiagnosticCategoryEnabled(int category, bool enabled)
{
    if (category < 0 || category >= static_cast<int>(topology_diagnostic_category_enabled_.size()))
        return;
    topology_diagnostic_category_enabled_[static_cast<size_t>(category)] = enabled;
    for (auto& [id, actor] : component_actors_) {
        actor->topologyDiagnostics().setCategoryEnabled(
            static_cast<GeometryTopologyDiagnosticCategory>(category), enabled);
    }
    if (category == static_cast<int>(GeometryTopologyDiagnosticCategory::InterferingFace))
        interference_->setEnabled(enabled);
}

void GeometryActorManager::setTopologyDiagnosticSmallEdgeLength(double threshold)
{
    if (!std::isfinite(threshold) || threshold <= 0.0)
        return;
    topology_diagnostic_small_edge_length_ = threshold;
    for (auto& [id, actor] : component_actors_)
        actor->topologyDiagnostics().setSmallEdgeLengthThreshold(threshold);
}

void GeometryActorManager::setTopologyDiagnosticSmallFaceArea(double threshold)
{
    if (!std::isfinite(threshold) || threshold <= 0.0)
        return;
    topology_diagnostic_small_face_area_ = threshold;
    for (auto& [id, actor] : component_actors_)
        actor->topologyDiagnostics().setSmallFaceAreaThreshold(threshold);
}
