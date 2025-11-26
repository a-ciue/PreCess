#include "SplineActor.h"
#include "Core.h"
#include <IVTKTools_ShapeDataSource.hxx>
#include <TopoDS_Shape.hxx>
#include <vtkPolyDataMapper.h>
#include <vtkRenderer.h>

SplineActor::SplineActor(vtkRenderer* renderer, SplineRenderMode render_mode)
{
    this->renderer_ = renderer;
    this->render_mode_ = render_mode;
}

SplineRenderMode SplineActor::getSplineRenderMode()
{
    return this->render_mode_;
}

bool SplineActor::getIsEdgeRender()
{
    return this->edge_render;
}

void SplineActor::loadShape(const SplineDataVtk& spline_data)
{
    this->spline_data_ = std::make_unique<SplineDataVtk>(spline_data);
    IVtkOCC_Shape::Handle aShapeImpl = new IVtkOCC_Shape(spline_data.shape);
    vtkSmartPointer<IVtkTools_ShapeDataSource> DS = vtkSmartPointer<IVtkTools_ShapeDataSource>::New();
    DS->SetShape(aShapeImpl);

    this->mapper_->SetInputConnection(DS->GetOutputPort());
    this->actor_->SetMapper(this->mapper_);

    this->renderer_->AddActor(this->actor_);
}

void SplineActor::deleteSplineActor()
{
    if (this->renderer_) {
        renderer_->RemoveActor(this->actor_);
    }
}

void SplineActor::setVisibility(bool visibility)
{
    this->actor_->SetVisibility(visibility);
}

void SplineActor::setRenderMode(SplineRenderMode render_mode)
{
}
