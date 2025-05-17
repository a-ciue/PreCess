#ifndef SPLINE_ACTOR_H
#define SPLINE_ACTOR_H
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <vtkNew.h>
#include <vtkAssembly.h>
#include <vtkPropAssembly.h>
#include <vtkMinimalStandardRandomSequence.h>
#include <vtkNamedColors.h>
#include "Core.h"
#include "ModelActor.h"
#include <vtkMultiBlockDataSet.h>
#include <vtkCompositePolyDataMapper.h>
#include <iostream>
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


class SplineActor {

public:
	SplineActor(vtkRenderer* renderer, SplineRenderMode render_mode);
	~SplineActor();

	void loadShape(const TopoDS_Shape &shape);
	void setVisibility(bool visibility);
	void setRenderMode(SplineRenderMode render_mode);

private:
	SplineRenderMode render_mode_;
	bool visibility_;
	TopoDS_Shape shape_;

	vtkNew<vtkActor> actor_;
	vtkNew<vtkPolyDataMapper> mapper_;
	vtkRenderer* renderer_;
};

#endif