#include "ModelActor.h"
#include "Model.h"
#include <vtkRenderer.h>
#include <vtkActor.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkMinimalStandardRandomSequence.h>

#include "Style.h"

//! @brief 完成三个renderer的初始化，其中patch_actors_可以由model_->update_patches_and_actors帮助更新
//! @param model
//ModelActor::ModelActor(Model* model)
//    : model_(model)
//{
//    model_->update_patches();
//}

//void ModelActor::set_model(Model* model)
//{
//    model_ = model;
//}



void ModelActor::update_patch(int patch_id, std::vector<std::array<double,3>> points, std::vector<std::array<int,3>> triangles)
{
    assert(points.size() == triangles.size());
    vtkActor* patch_actor = patch_actors_[patch_id];

    // vtkPolyData
    vtkSmartPointer<vtkPoints> points_data = vtkSmartPointer<vtkPoints>::New();
    for (std::array<double, 3>& point : points) {
        points_data->InsertNextPoint(point.data());
    }

    // Get Triangles
    vtkSmartPointer<vtkCellArray> triangles_data = vtkSmartPointer<vtkCellArray>::New();
    for (std::array<int, 3>& triangle : triangles) {
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

}
