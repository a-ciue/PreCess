#include "GeometryActorManager.h"
#include "GeometryActor.h"
#include "GeometryTopologyDiagnosticActor.h"

#include "Core.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <spdlog/spdlog.h>

namespace {
/** @brief 生成日志使用的“id (名称)”标签，名称为空时只保留 id。 */
std::string componentLabel(Index id, const std::string& name)
{
    return std::to_string(id) + (name.empty() ? std::string() : " (" + name + ")");
}
}

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
    loadGeometry(geometry_data, {});
}

void GeometryActorManager::loadGeometry(
    const GeometryDataVtk& geometry_data, const std::string& component_name)
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
    actor_ptr->setSurfacePrecision(surface_precision_);
    // 先同步类别开关但不计算，避免复用 Actor 时对即将替换的旧 Shape 重建诊断。
    actor_ptr->topologyDiagnostics().setCategoryFlagsOnly(
        topology_diagnostic_category_enabled_);
    actor_ptr->topologyDiagnostics().setSmallEdgeLengthThreshold(
        topology_diagnostic_small_edge_length_);
    actor_ptr->topologyDiagnostics().setSmallFaceAreaThreshold(
        topology_diagnostic_small_face_area_);
    // 已启用的诊断会在 loadShape 中立即执行，因此必须先设置日志标签。
    actor_ptr->topologyDiagnostics().setComponentLabel(
        componentLabel(component_id, component_name));
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
        // 隐藏时切档会移除旧敏感实体，恢复显示时必须按当前样式重新启用拾取。
        op_.setShapePickingEnabled(actor, actor->isVisible());
    }
}

GeometryRenderStyle GeometryActorManager::getCurrentRenderStyle() const
{
    return current_style_;
}

void GeometryActorManager::setSurfacePrecision(int precision)
{
    if (!surface_precision::isValid(precision) || precision == surface_precision_)
        return;
    // 所有三角化及诊断均在独立副本上完成，准备失败时连旧 Picker 缓存也不触碰。
    struct PreparedComponent {
        Index id;
        std::shared_ptr<GeometryActor> actor;
        std::unique_ptr<GeometryActor> display;
    };
    std::vector<PreparedComponent> prepared;
    prepared.reserve(component_actors_.size());
    for (const auto& [id, actor] : component_actors_)
        prepared.push_back({ id, actor, actor->prepareSurfacePrecision(precision) });

    size_t committed = 0;
    try {
        for (auto& component : prepared) {
            // Picker 敏感实体引用旧拓扑，必须在交换前移除、交换后重新创建。
            op_.unregisterProps(component.actor);
            component.actor->swapDisplay(*component.display);
            ++committed;
            op_.registerProps(component.id, component.actor);
        }
    } catch (...) {
        while (committed > 0) {
            auto& component = prepared[--committed];
            op_.unregisterProps(component.actor);
            component.actor->swapDisplay(*component.display);
            op_.registerProps(component.id, component.actor);
        }
        throw;
    }
    surface_precision_ = precision;
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
