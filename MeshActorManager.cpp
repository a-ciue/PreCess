#include "MeshActorManager.h"
#include "MeshActorManager.h"
#include "MeshActorManager.h"
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <vtkNew.h>
#include <vtkAssembly.h>
#include <vtkPropAssembly.h>
#include <vtkMinimalStandardRandomSequence.h>
#include <vtkNamedColors.h>
#include "Core.h"
#include "MeshActor.h"
#include <vtkMultiBlockDataSet.h>
#include <vtkCompositePolyDataMapper.h>
#include <iostream>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQml/qqmlregistration.h>
#include <vtkSphereSource.h>
#include <vtkOBJReader.h>
#include <QQuickVTKItem.h>
#include <QVTKRenderWindowAdapter.h>

#include <vtkActor.h>
#include <vtkCellArray.h>
#include <vtkMinimalStandardRandomSequence.h>
#include <vtkNamedColors.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkAssembly.h>
#include <vtkPropAssembly.h>
#include "ModelUtil.h"
#include "Style.h"
#include "Core.h"
#include <vtkAppendPolyData.h>
#include <vtkCompositePolyDataMapper.h>
#include <vtkUnstructuredGrid.h>
#include <vtkTriangle.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkUnsignedCharArray.h>  
#include <vtkCellData.h>           
#include <cstdlib>    
#include <iostream>

const MeshActor* MeshActorManager::getModelActor(Index model_id)
{
    if (!this->models_.count(model_id))
	return this->models_.at(model_id).get();
}

void MeshActorManager::deleteModel(Index model_id)
{
    if (!this->models_.count(model_id))
	this->models_.erase(model_id);
}

void MeshActorManager::bindRender(vtkRenderer* renderer)
{
    this->renderer_ = renderer;
}

void MeshActorManager::loadModel(Index model_id, MeshDataVtk model_data, vtkRenderer* renderer, ModelRenderMode render_mode, bool edge_render)
{
	if (!this->models_.count(model_id))
		this->models_[model_id] = std::make_unique<MeshActor>(renderer, edge_render, render_mode);
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

bool MeshActorManager::getCount(Index model_id)
{
    return this->models_.count(model_id);
}

bool MeshActorManager::getIsEdgeRender(Index model_id)
{
    if (this->models_.count(model_id))
    return this->models_[model_id]->getIsEdgeRender();
}
