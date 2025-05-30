#include "SplineActorManager.h"
#include "SplineActorManager.h"
#include "SplineActorManager.h"
#include "SplineActorManager.h"
#include "SplineActorManager.h"
#include "SplineActorManager.h"
#include "SplineActorManager.h"
#include "SplineActorManager.h"
#include "SplineActorManager.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <TopoDS_Shape.hxx>
#include <BRepTools.hxx>
#include <STEPControl_Reader.hxx>
#include <vtkRenderWindow.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkActor.h>
#include <vtkPolyDataMapper.h>
#include <vtkNew.h>
#include <vtkRenderer.h>
#include <iostream>
#include <IVTKTools_ShapeDataSource.hxx>
#include <vtkNew.h>
#include <vtkAssembly.h>
#include <vtkPropAssembly.h>
#include <vtkMinimalStandardRandomSequence.h>
#include <vtkNamedColors.h>
#include "Core.h"
#include "SplineActor.h"
#include <vtkMultiBlockDataSet.h>
#include <vtkCompositePolyDataMapper.h>
#include <iostream>
#include <vtkRenderWindow.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkActor.h>
#include <vtkPolyDataMapper.h>
#include <vtkNew.h>
#include <vtkRenderer.h>
#include <iostream>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQml/qqmlregistration.h>
#include <vtkSphereSource.h>
#include <vtkOBJReader.h>
#include <QQuickVTKItem.h>
#include <QVTKRenderWindowAdapter.h>

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

void SplineActorManager::loadSpline(Index model_id, TopoDS_Shape shape)
{
	if (!this->models_.count(model_id))
		this->models_[model_id] = std::make_unique<SplineActor>(this->renderer_,SplineRenderMode::Face);
	this->models_[model_id]->loadShape(shape);
}

void SplineActorManager::setVisibility(Index model_id, bool visibility)
{
	if (this->models_.count(model_id))
	this->models_[model_id]->setVisibility(visibility);
}

Q_INVOKABLE void SplineActorManager::setRenderMode(Index model_id, SplineRenderMode render_mode)
{
	if (this->models_.count(model_id))
	this->models_[model_id]->setRenderMode(render_mode);
	
}
