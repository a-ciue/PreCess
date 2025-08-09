#include "SplineActorManager.h"
#include "SplineActor.h"

#include "Core.h"

SplineActorManager::SplineActorManager() = default;

SplineActorManager::~SplineActorManager() = default;

void SplineActorManager::bindRender(vtkRenderer* renderer)
{
	this->renderer_ = renderer;
}

const SplineActor* SplineActorManager::getSplineActor(Index model_id)
{
	if (this->models_.count(model_id))
		return this->models_.at(model_id).get();
	else
	{
		std::cout << "SplineActorManager getSplineActor error" << std::endl;
		return nullptr;
	}
	
}

SplineRenderMode SplineActorManager::getSplineRenderMode(Index model_id)
{
	if (this->models_.count(model_id))
	return this->models_[model_id]->getSplineRenderMode();
}

bool SplineActorManager::getIsEdgeRender(Index model_id)
{
	if (this->models_.count(model_id))
	return  this->models_[model_id]->getIsEdgeRender();
}

bool SplineActorManager::getCount(Index model_id)
{
	return this->models_.count(model_id);
}

void SplineActorManager::deleteModel(Index model_id)
{
	if (this->models_.count(model_id))
	{
		this->models_[model_id]->deleteSplineActor();
		this->models_.erase(model_id);
	}
	
}

void SplineActorManager::loadSpline(Index model_id, const SplineDataVtk& spline_data)
{
	if (!this->models_.count(model_id))
		this->models_[model_id] = std::make_unique<SplineActor>(this->renderer_,SplineRenderMode::Face);
	this->models_[model_id]->loadShape(spline_data);
}

void SplineActorManager::setVisibility(Index model_id, bool visibility)
{
	if (this->models_.count(model_id))
	this->models_[model_id]->setVisibility(visibility);
}

void SplineActorManager::setRenderMode(Index model_id, SplineRenderMode render_mode)
{
	if (this->models_.count(model_id))
	this->models_[model_id]->setRenderMode(render_mode);
	
}
