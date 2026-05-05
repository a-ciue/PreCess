#include "GeometryActorManager.h"
#include "GeometryActor.h"

#include "Core.h"

#include <algorithm>
#include <iostream>

GeometryActorManager::GeometryActorManager() = default;

GeometryActorManager::~GeometryActorManager() = default;

void GeometryActorManager::bindRender(vtkRenderer* renderer)
{
    this->renderer_ = renderer;
}

const GeometryActor* GeometryActorManager::getSplineActor(Index component_id)
{
    auto it = component_actors_.find(component_id);
    if (it != component_actors_.end()) {
        return it->second.get();
    } else {
        std::cout << "GeometryActorManager getSplineActor error" << std::endl;
        return nullptr;
    }
}

SplineRenderMode GeometryActorManager::getSplineRenderMode(Index component_id)
{
    auto it = component_actors_.find(component_id);
    if (it != component_actors_.end()) {
        return it->second->getSplineRenderMode();
    }

    std::cout << "get spline render mode error" << std::endl;
    return SplineRenderMode::Face;
}

bool GeometryActorManager::getIsEdgeRender(Index component_id)
{
    auto it = component_actors_.find(component_id);
    if (it != component_actors_.end()) {
        return it->second->getIsEdgeRender();
    }

    std::cout << "get is edge render mode error" << std::endl;
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

void GeometryActorManager::loadSpline(const GeometryDataVtk& spline_data)
{
    Index component_id = spline_data.component_id;

    if (!component_actors_.count(component_id)) {
        component_actors_[component_id] = std::make_unique<GeometryActor>(this->renderer_, SplineRenderMode::Face);
    }

    component_actors_[component_id]->loadShape(spline_data);
}

void GeometryActorManager::setVisibility(Index component_id, bool visibility)
{
    auto it = component_actors_.find(component_id);
    if (it != component_actors_.end()) {
        it->second->setVisibility(visibility);
    }
}

void GeometryActorManager::setRenderMode(Index component_id, SplineRenderMode render_mode)
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