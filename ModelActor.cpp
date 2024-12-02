#include "ModelActor.h"
#include "Model.h"
#include <vtkActor.h>
#include <vtkCellArray.h>
#include <vtkMinimalStandardRandomSequence.h>
#include <vtkNamedColors.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

#include "ModelUtil.h"
#include "Style.h"

//! @brief 完成三个renderer的初始化，其中patch_actors_可以由model_->update_patches_and_actors帮助更新
//! @param model
// ModelActor::ModelActor(Model* model)
//     : model_(model)
//{
//     model_->update_patches();
// }

// void ModelActor::set_model(Model* model)
//{
//     model_ = model;
// }

void ModelActor::update_patch(int patch_id, const std::vector<double[3]>& points, const std::vector<int[3]>& triangles)
{
    assert(points.size() == triangles.size());
    vtkActor* patch_actor = patch_actors_[patch_id];

    // vtkPolyData
    vtkSmartPointer<vtkPoints> points_data = vtkSmartPointer<vtkPoints>::New();
    for (const double(&point)[3] : points) {
        points_data->InsertNextPoint(point);
    }

    vtkSmartPointer<vtkCellArray> triangles_data = vtkSmartPointer<vtkCellArray>::New();
    for (const int(&triangle)[3] : triangles) {
        vtkIdType triangle_idxs[3] { triangle[0], triangle[1], triangle[2] };
        triangles_data->InsertNextCell(3, triangle_idxs);
    }

    vtkNew<vtkPolyData> vtkData;
    vtkData->SetPoints(points_data);
    vtkData->SetPolys(triangles_data);

    // Mapper & Actor
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(vtkData);
    patch_actor->SetMapper(mapper);

    double r = ModelUtil::randomSequence.GetNextRangeValue(0.1, 0.6),
           g = ModelUtil::randomSequence.GetNextRangeValue(0.1, 0.6),
           b = ModelUtil::randomSequence.GetNextRangeValue(0.1, 0.6);

    patch_actor->GetProperty()->SetDiffuseColor(r, g, b);
    patch_actor->GetProperty()->SetDiffuse(0.8);
    patch_actor->GetProperty()->SetSpecular(0.5);
    patch_actor->GetProperty()->SetSpecularColor(
        ModelUtil::colors.GetColor3d("White").GetData());
    patch_actor->GetProperty()->SetSpecularPower(30.0);
}

void ModelActor::update_block(int block_id)
{
    const std::vector<int>& patch_ids = model_->block_patch_ids(block_id);

    for (int patch_id : patch_ids)
    {
        vtkActor* patch_actor = patch_actors_[patch_id];
        patch_actor->GetMapper()->GetInput()
    }

    if (block_renderer_)
    {

    }
}
