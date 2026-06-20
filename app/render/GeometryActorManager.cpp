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

const GeometryActor* GeometryActorManager::getGeometryActor(Index component_id)
{
    auto it = component_actors_.find(component_id);
    if (it != component_actors_.end()) {
        return it->second.get();
    } else {
        spdlog::error("GeometryActorManager getGeometryActor error");
        return nullptr;
    }
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
    component_actors_.erase(component_id);
}

void GeometryActorManager::loadGeometry(const GeometryDataVtk& geometry_data)
{
    Index component_id = geometry_data.component_id;

    if (!component_actors_.count(component_id)) {
        component_actors_[component_id] = std::make_unique<GeometryActor>(this->renderer_, GeometryRenderMode::Face);
    }

    component_actors_[component_id]->loadShape(geometry_data);
}

void GeometryActorManager::setVisibility(Index component_id, bool visibility)
{
    auto it = component_actors_.find(component_id);
    if (it != component_actors_.end()) {
        it->second->setVisibility(visibility);
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