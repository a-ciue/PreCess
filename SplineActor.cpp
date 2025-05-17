#include "SplineActor.h"
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

SplineActor::SplineActor(vtkRenderer* renderer, SplineRenderMode render_mode)
{
    this->renderer_ = renderer;
    this->render_mode_ = render_mode;
}

SplineActor::~SplineActor()
{
    if (this->renderer_)
    {
        renderer_->RemoveActor(this->actor_);
    }
}

void SplineActor::loadShape(const TopoDS_Shape& shape)
{
    this->shape_ = shape_;
    IVtkOCC_Shape::Handle aShapeImpl = new IVtkOCC_Shape(shape);
    vtkSmartPointer<IVtkTools_ShapeDataSource> DS = vtkSmartPointer<IVtkTools_ShapeDataSource>::New();
    DS->SetShape(aShapeImpl);

    this->mapper_->SetInputConnection(DS->GetOutputPort());
    this->actor_->SetMapper(this->mapper_);

    this->renderer_->AddActor(this->actor_);
}

void SplineActor::setVisibility(bool visibility)
{
    this->actor_->SetVisibility(visibility);
}

void SplineActor::setRenderMode(SplineRenderMode render_mode)
{

}



