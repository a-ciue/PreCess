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

const MeshActor* ModelActorManager::getModelActor(Index model_id)
{
	return this->models_.at(model_id).get();
}

void ModelActorManager::deleteModel(Index model_id)
{
	this->models_.erase(model_id);
}

void ModelActorManager::loadModel(Index model_id, MeshDataVtk model_data, vtkRenderer* renderer, ModelRenderMode render_mode)
{
        if (!this->models_.count(model_id))
            this->models_[model_id] = std::make_unique<MeshActor>(renderer, this->edge_render_, this->render_mode_);
        this->models_[model_id]->loadModelData(model_data);
        this->models_[model_id]->setRenderMode(render_mode);
}

void ModelActorManager::setVisibility(Index model_id, bool visibility)
{
    this->models_[model_id]->setVisibility(visibility);
}

void ModelActorManager::setRenderMode(Index model_id, ModelRenderMode render_mode)
{
    this->models_[model_id]->setRenderMode(render_mode);
}

void ModelActorManager::setRenderEdge(Index model_id, bool is_render)
{
    this->models_[model_id]->setRenderEdge(is_render);
}

bool ModelActorManager::getCount(Index model_id)
{
    return this->models_.count(model_id);
}

bool ModelActorManager::getIsEdgeRender(Index model_id)
{
    return this->models_[model_id]->getIsEdgeRender();
}
