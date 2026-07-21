#include "GeometryActorManager.h"
#include "GeometryActor.h"

#include "Core.h"

#include <algorithm>
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

GeometryRenderMode GeometryActorManager::getGeometryRenderMode(Index component_id)
{
    auto it = component_actors_.find(component_id);
    if (it != component_actors_.end()) {
        return it->second->getGeometryRenderMode();
    }

    spdlog::error("get geometry render mode error");
    return GeometryRenderMode::Face;
}

bool GeometryActorManager::getIsEdgeRender(Index component_id)
{
    auto it = component_actors_.find(component_id);
    if (it != component_actors_.end()) {
        return it->second->getIsEdgeRender();
    }

    spdlog::error("get is edge render mode error");
    return false;
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
        component_actors_[component_id] = std::make_shared<GeometryActor>(this->renderer_, GeometryRenderMode::Face);
    } else {
        // 先用旧 OCC Shape 注销 Picker 状态，再由 loadShape 替换几何数据。
        op_.unregisterProps(actor_it->second);
    }

    auto& actor_ptr = component_actors_[component_id];
    actor_ptr->loadShape(geometry_data);
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

void GeometryActorManager::setRenderMode(Index component_id, GeometryRenderMode render_mode)
{
    auto it = component_actors_.find(component_id);
    if (it != component_actors_.end()) {
        it->second->setRenderMode(render_mode);
    }
}

void GeometryActorManager::setRenderEdge(Index component_id, bool is_render)
{
    auto it = component_actors_.find(component_id);
    if (it != component_actors_.end()) {
        it->second->setRenderEdge(is_render);
    }
}
