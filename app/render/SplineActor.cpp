#include "SplineActor.h"
#include "Core.h"
#include <IVTKTools_ShapeDataSource.hxx>
#include <TopoDS_Shape.hxx>
#include <vtkPointData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
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
    DS->Update();
    vtkPolyData* spline_poly_data = DS->GetOutput();

    // 分开 polys 和 lines
    vtkNew<vtkPolyData> line_only;
    line_only->SetPoints(spline_poly_data->GetPoints());
    line_only->SetLines(spline_poly_data->GetLines());

    vtkNew<vtkPolyData> poly_only;
    poly_only->SetPoints(spline_poly_data->GetPoints());
    poly_only->SetPolys(spline_poly_data->GetPolys());
    poly_only->GetPointData()->SetNormals(spline_poly_data->GetPointData()->GetNormals());

    // 渲染面（带光照）
    vtkNew<vtkPolyDataMapper> poly_mapper;
    poly_mapper->SetInputData(poly_only);

    this->poly_actor_->SetMapper(poly_mapper);
    this->renderer_->AddActor(this->poly_actor_);

    // 渲染线（无光照）
    vtkNew<vtkPolyDataMapper> line_mapper;
    line_mapper->SetInputData(line_only);
    line_mapper->SetRelativeCoincidentTopologyLineOffsetParameters(0, -0.1);

    this->line_actor_->SetMapper(line_mapper);
    this->line_actor_->GetProperty()->LightingOff(); // 关键：关闭光照防止线变色
    this->line_actor_->GetProperty()->SetColor(0.0, 0.0, 0.0);
    this->line_actor_->GetProperty()->SetLineWidth(2.0);
    this->line_actor_->GetProperty()->RenderLinesAsTubesOn(); // 关键：线条抗锯齿
    this->renderer_->AddActor(this->line_actor_);
}

void SplineActor::deleteSplineActor()
{
    if (this->renderer_) {
        renderer_->RemoveActor(this->poly_actor_);
        renderer_->RemoveActor(this->line_actor_);
    }
}

void SplineActor::setVisibility(bool visibility)
{
    this->poly_actor_->SetVisibility(visibility);
    this->line_actor_->SetVisibility(visibility);
    this->visibility_ = visibility;
}

void SplineActor::setRenderMode(SplineRenderMode render_mode)
{
}
