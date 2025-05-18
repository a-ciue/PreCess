#ifndef SPLINE_ACTOR_MANAGER_H
#define SPLINE_ACTOR_MANAGER_H
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
#include "MeshActor.h"
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

class SplineActorManager
{
public:
	void bindRender(vtkRenderer* renderer);
	const SplineActor* getSplineActor(Index model_id);
	void deleteModel(Index model_id);
	void loadSpline(Index model_id, SplineDataVtk spline_data);

	void setVisibility(Index model_id, bool visibility);
	void setRenderMode(Index model_id, SplineRenderMode render_mode);

private:
	std::unordered_map <Index, std::unique_ptr<SplineActor>> models_;
	vtkRenderer* renderer_;
};


#endif