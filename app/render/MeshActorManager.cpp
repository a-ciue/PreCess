#include "MeshActorManager.h"
#include "Core.h"
#include "renderStrategy/AttriRenderStrategyScalar.h"
#include "renderStrategy/AttriRenderStrategyVector.h"
#include "renderStrategy/AttriRenderStrategyUV.h"
#include "renderStrategy/AttriRenderStrategyRGB.h"
#include <spdlog/spdlog.h>

MeshActorManager::MeshActorManager(vtkPoints* global_points)
    : global_points_(global_points)
{
}

std::shared_ptr<MeshActor> MeshActorManager::getComponentActor(Index component_id) const
{
    if (this->component_actors_.count(component_id))
        return this->component_actors_.at(component_id);
    else {
        spdlog::error("MeshActorManager getComponentActor error");
        return nullptr;
    }
}

void MeshActorManager::deleteComponent(Index component_id)
{
    if (this->component_actors_.count(component_id)) {
        this->component_actors_.erase(component_id);
    }
}

void MeshActorManager::bindRender(vtkRenderer* renderer)
{
    this->renderer_ = renderer;
}

bool MeshActorManager::hasComponent(Index component_id) const
{
    return this->component_actors_.count(component_id) != 0;
}

void MeshActorManager::loadMesh(Index component_id, const MeshDataVtk& model_data, vtkRenderer* renderer, ModelRenderMode render_mode)
{
    if (!this->component_actors_.count(component_id))
        this->component_actors_[component_id] = std::make_shared<MeshActor>(renderer, global_points_);

    auto& actor = this->component_actors_[component_id];
    actor->loadModelData(model_data);
    actor->setRenderMode(render_mode);
}

void MeshActorManager::setVisibility(Index component_id, bool visibility)
{
    if (this->component_actors_.count(component_id))
        this->component_actors_[component_id]->setVisibility(visibility);
}

void MeshActorManager::setRenderMode(Index component_id, ModelRenderMode render_mode)
{
    if (this->component_actors_.count(component_id))
        this->component_actors_[component_id]->setRenderMode(render_mode);
}

void MeshActorManager::setRenderEdge(Index component_id, bool is_render)
{
    if (this->component_actors_.count(component_id))
        this->component_actors_[component_id]->setRenderEdge(is_render);
}

void MeshActorManager::setClipPlane(vtkPlane* plane)
{
    for (auto&& [idx, mesh_actor] : this->component_actors_) {
        mesh_actor->setClipPlane(plane);
    }
}

bool MeshActorManager::getCount(Index component_id)
{
    return this->component_actors_.count(component_id);
}


bool MeshActorManager::getIsEdgeRender(Index component_id)
{
    if (this->component_actors_.count(component_id))
        return this->component_actors_[component_id]->getIsEdgeRender();
    return false;
}

ModelRenderMode MeshActorManager::getMeshRenderMode(Index component_id)
{
    if (this->component_actors_.count(component_id))
        return this->component_actors_[component_id]->getMeshRenderMode();

    return ModelRenderMode::Face;
}

void MeshActorManager::setAttriMode(
    Index component_id,
    const std::string& attr_name,
    Mode mode,
    std::map<std::string, std::any> args)
{
    if (this->component_actors_.count(component_id)) {
        std::unique_ptr<IAttributeRenderStrategy> strategy;
        switch (mode) {
        case Mode::SCALAR:
            strategy = std::make_unique<AttriRenderStrategyScalar>();
            break;
        case Mode::VECTOR:
            strategy = std::make_unique<AttriRenderStrategyVector>();
            break;
        case Mode::UV:
            strategy = std::make_unique<AttriRenderStrategyUV>();
            break;
        case Mode::RGB:
            strategy = std::make_unique<AttriRenderStrategyRGB>();
            break;
        default:
            spdlog::error("Invalid attribute render mode");
            return;
        }
        this->component_actors_[component_id]->setRenderStrategy(std::move(strategy));
        this->component_actors_[component_id]->renderAttribute(attr_name, args);
    }
}

void MeshActorManager::cancelAttri(Index component_id)
{
    if (this->component_actors_.count(component_id))
        this->component_actors_[component_id]->cancelActiveAttribute();
}

void MeshActorManager::syncOriginalPointIds()
{
    for (auto& [cid, actor] : component_actors_) {
        if (!actor)
            continue;
        actor->ensureOriginalPointIds(); 
    }
}