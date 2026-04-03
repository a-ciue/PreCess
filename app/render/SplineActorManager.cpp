#include "SplineActorManager.h"
#include "SplineActor.h"

#include "Core.h"

#include <algorithm>
#include <iostream>

SplineActorManager::SplineActorManager() = default;

SplineActorManager::~SplineActorManager() = default;

void SplineActorManager::bindRender(vtkRenderer* renderer)
{
    this->renderer_ = renderer;
}

const SplineActor* SplineActorManager::getSplineActor(Index component_id)
{
    auto it = component_actors_.find(component_id);
    if (it != component_actors_.end()) {
        return it->second.get();
    } else {
        std::cout << "SplineActorManager getSplineActor error" << std::endl;
        return nullptr;
    }
}

SplineRenderMode SplineActorManager::getSplineRenderMode(Index component_id)
{
    auto it = component_actors_.find(component_id);
    if (it != component_actors_.end()) {
        return it->second->getSplineRenderMode();
    }

    std::cout << "get spline render mode error" << std::endl;
    return SplineRenderMode::Face;
}

bool SplineActorManager::getIsEdgeRender(Index component_id)
{
    auto it = component_actors_.find(component_id);
    if (it != component_actors_.end()) {
        return it->second->getIsEdgeRender();
    }

    std::cout << "get is edge render mode error" << std::endl;
    return false;
}

bool SplineActorManager::hasComponent(Index component_id) const
{
    return component_actors_.count(component_id) != 0;
}

void SplineActorManager::deleteComponent(Index component_id)
{
    component_actors_.erase(component_id);
}

void SplineActorManager::loadSpline(const SplineDataVtk& spline_data)
{
    Index component_id = spline_data.component_id;

    if (!component_actors_.count(component_id)) {
        component_actors_[component_id] = std::make_unique<SplineActor>(this->renderer_, SplineRenderMode::Face);
    }

    component_actors_[component_id]->loadShape(spline_data);
}

void SplineActorManager::setVisibility(Index component_id, bool visibility)
{
    auto it = component_actors_.find(component_id);
    if (it != component_actors_.end()) {
        it->second->setVisibility(visibility);
    }
}

void SplineActorManager::setRenderMode(Index component_id, SplineRenderMode render_mode)
{
    auto it = component_actors_.find(component_id);
    if (it != component_actors_.end()) {
        it->second->setRenderMode(render_mode);
    }
}

void SplineActorManager::setRenderEdge(Index component_id, bool is_render)
{
    auto it = component_actors_.find(component_id);
    if (it != component_actors_.end()) {
        it->second->setRenderEdge(is_render);
    }
}