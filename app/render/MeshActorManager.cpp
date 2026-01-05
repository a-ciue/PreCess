#include "MeshActorManager.h"
#include "Core.h"

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
    std::string attr_name,
    Mode mode,
    ElementType type,
    std::string texture_path,
    double glyph_scale,
    std::optional<std::pair<double, double>> scalar_range
    )
{
    if (this->models_.count(model_id))
        this->models_[model_id]->setAttriMode(attr_name, mode, type, texture_path,glyph_scale,scalar_range);
}
void MeshActorManager::cancelAttri(Index model_id)
{
    if (this->models_.count(model_id))
        this->models_[model_id]->cancelActiveAttribute();
}

