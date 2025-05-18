#ifndef MODEL_ACTOR_MANAGER_H
#define MODEL_ACTOR_MANAGER_H
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


struct Group;
struct Block;
struct Patch;
class vtkActor;
class vtkRenderer;
class vtkPolyData;
class Model;


class ModelActorManager :QObject
{
public:
	const MeshActor* getModelActor(Index model_id);
	void deleteModel(Index model_id);
	void loadModel(Index model_id, MeshDataVtk model_data, vtkRenderer* renderer, ModelRenderMode render_mode);

	void setVisibility(Index model_id, bool visibility);
	Q_INVOKABLE void setRenderMode(Index model_id, ModelRenderMode render_mode);
	Q_INVOKABLE void setRenderEdge(Index model_id, bool is_render);

	bool getCount(Index model_id);
	bool getIsEdgeRender(Index model_id);
private:
	std::unordered_map<Index, std::unique_ptr<MeshActor>> models_;
	bool edge_render_;
	ModelRenderMode render_mode_;
	vtkRenderer* renderer_;
};
#endif