#include "GeometryActorManager.h"
#include "GeometryActor.h"
#include "GeometryTopologyDiagnosticActor.h"

#include "Core.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <spdlog/spdlog.h>

GeometryActorManager::GeometryActorManager() = default;

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
    auto it = component_actors_.find(component_id);
    if (it != component_actors_.end()) {
        op_.unregisterProps(it->second);
        component_actors_.erase(it);
    }
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
    for (size_t category = 0; category < topology_diagnostic_category_enabled_.size(); ++category) {
        actor_ptr->topologyDiagnostics().setCategoryEnabled(
            static_cast<GeometryTopologyDiagnosticCategory>(category),
            topology_diagnostic_category_enabled_[category]);
    }
    actor_ptr->topologyDiagnostics().setCleanupTolerance(topology_diagnostic_tolerance_);
    actor_ptr->loadShape(geometry_data);
    actor_ptr->setRenderStyle(current_style_);
    op_.registerProps(component_id, actor_ptr);
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
}

void GeometryActorManager::setTopologyDiagnosticTolerance(double tolerance)
{
    if (!std::isfinite(tolerance) || tolerance <= 0.0)
        return;
    topology_diagnostic_tolerance_ = tolerance;
    for (auto& [id, actor] : component_actors_)
        actor->topologyDiagnostics().setCleanupTolerance(tolerance);
}
