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
#include "MeshActor.h"
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
#include "core/SplineDataVtk.h"

class SplineActor {

public:
	SplineActor(vtkRenderer* renderer, SplineRenderMode render_mode);

	SplineRenderMode getSplineRenderMode();
	bool getIsEdgeRender();

	void loadShape(const SplineDataVtk& spline_data);
	void deleteSplineActor();

	void setVisibility(bool visibility);
	void setRenderMode(SplineRenderMode render_mode);

private:
	SplineRenderMode render_mode_;
	bool edge_render;
	bool visibility_;
	SplineDataVtk spline_data_;

	vtkNew<vtkActor> actor_;
	vtkNew<vtkPolyDataMapper> mapper_;
	vtkRenderer* renderer_;
};

#endif