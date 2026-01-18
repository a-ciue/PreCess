#include "MeshActorManager.h"
#include "Core.h"
#include "renderStrategy/AttriRenderStrategyScalar.h"
#include "renderStrategy/AttriRenderStrategyVector.h"
#include "renderStrategy/AttriRenderStrategyUV.h"
#include "renderStrategy/AttriRenderStrategyRGB.h"
#include <spdlog/spdlog.h>
std::shared_ptr<const MeshActor> MeshActorManager::getModelActor(Index model_id)
{
    if (this->models_.count(model_id))
        return this->models_.at(model_id);
    else {
        std::cout << "MeshActorManager getModelActor error" << std::endl;
        return nullptr;
    }
}

void MeshActorManager::deleteModel(Index model_id)
{
    if (this->models_.count(model_id)) {
        this->models_.erase(model_id);
    }
}

void MeshActorManager::bindRender(vtkRenderer* renderer)
{
    this->renderer_ = renderer;
}

void MeshActorManager::loadModel(Index model_id, const MeshDataVtk& model_data, vtkRenderer* renderer, ModelRenderMode render_mode)
{
    if (!this->models_.count(model_id))
        this->models_[model_id] = std::make_unique<MeshActor>(renderer);
    this->models_[model_id]->loadModelData(model_data);
    this->models_[model_id]->setRenderMode(render_mode);
}

void MeshActorManager::setVisibility(Index model_id, bool visibility)
{
    if (this->models_.count(model_id))
        this->models_[model_id]->setVisibility(visibility);
}

void MeshActorManager::setRenderMode(Index model_id, ModelRenderMode render_mode)
{
    if (this->models_.count(model_id))
        this->models_[model_id]->setRenderMode(render_mode);
}

void MeshActorManager::setRenderEdge(Index model_id, bool is_render)
{
    if (this->models_.count(model_id))
        this->models_[model_id]->setRenderEdge(is_render);
}

void MeshActorManager::setRenderVertex(Index model_id, bool is_render)
{
    if (this->models_.count(model_id))
        this->models_[model_id]->setRenderVertex(is_render);
}

void MeshActorManager::setClipPlane(vtkPlane* plane)
{
    for (auto&& [idx, mesh_actor] : this->models_) {
        mesh_actor->setClipPlane(plane);
    }
}

bool MeshActorManager::getCount(Index model_id)
{
    return this->models_.count(model_id);
}

bool MeshActorManager::getIsEdgeRender(Index model_id)
{
    if (this->models_.count(model_id))
        return this->models_[model_id]->getIsEdgeRender();
}

bool MeshActorManager::getIsVertexRender(Index model_id)
{
    if (this->models_.count(model_id))
        return this->models_[model_id]->getIsVertexRender();
    return false;
}

ModelRenderMode MeshActorManager::getMeshRenderMode(Index model_id)
{
    if (this->models_.count(model_id))
        return this->models_[model_id]->getMeshRenderMode();
}
void MeshActorManager::setAttriMode(
    Index model_id,
    const std::string& attr_name,
    Mode mode,
    std::map<std::string, std::any> args)
{
    if (this->models_.count(model_id)) {
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
        this->models_[model_id]->setRenderStrategy(std::move(strategy));
        this->models_[model_id]->renderAttribute(attr_name, args);
    }
}
void MeshActorManager::cancelAttri(Index model_id)
{
    if (this->models_.count(model_id))
        this->models_[model_id]->cancelActiveAttribute();
}

