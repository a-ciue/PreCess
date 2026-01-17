#include "AttributeOperator.h"
#include <vtkPolyData.h>
#include <vtkCellCenters.h>
#include <vtkCellData.h>
#include <vtkSmartPointer.h>
#include <cassert>
#include <vtkPointData.h> 
AttributeOperator::AttributeOperator(MeshActor* mesh_actor)
    : mesh_actor_(mesh_actor) {};

vtkPolyDataMapper* AttributeOperator::getVertexMapper() { 
    return mesh_actor_->vertex_mapper_; 
}

vtkPolyDataMapper* AttributeOperator::getFaceMapper() { 
    return mesh_actor_->face_mapper_; 
}

vtkPolyDataMapper* AttributeOperator::getGlyph3DMapper() { 
    return mesh_actor_->glyph3D_mapper_; 
}

vtkActor* AttributeOperator::getGlyph3DActor() { 
    return mesh_actor_->glyph3D_actor_; 
}

vtkActor* AttributeOperator::getFaceActor()
{
    return mesh_actor_->face_actor_;
}

vtkCellData* AttributeOperator::getFaceCellData()
{ 
    return mesh_actor_->face_data_->GetCellData();
}

vtkPointData* AttributeOperator::getFacePointData()
{
    return mesh_actor_->face_data_->GetPointData();
}

vtkPointData* AttributeOperator::getVertexPointData() { 
    return mesh_actor_->vertex_data_->GetPointData(); 
}

vtkSmartPointer<vtkPolyData> AttributeOperator::getFaceGlyphInput(const std::string& attr_name)
{
    vtkPolyData* face_data = mesh_actor_->face_data_;
    if (!face_data)
        return nullptr;

    vtkDataArray* array = face_data->GetCellData()->GetArray(attr_name.c_str());
    if (!array)
        return nullptr;

    // 计算面中心点位置
    vtkNew<vtkCellCenters> centers;
    centers->SetInputData(face_data);
    centers->Update();

    // 合并面中心点与向量数据
    vtkSmartPointer<vtkPolyData> glyphInput = vtkSmartPointer<vtkPolyData>::New();
    glyphInput->SetPoints(centers->GetOutput()->GetPoints());
    glyphInput->GetPointData()->SetVectors(array);

    return glyphInput;
}

vtkSmartPointer<vtkPolyData> AttributeOperator::getVertexGlyphInput(const std::string& attr_name)
{
    vtkPolyData* vertex_data = mesh_actor_->vertex_data_;
    vtkDataArray* array = vertex_data->GetPointData()->GetArray(attr_name.c_str());
    if (!array)
        return nullptr;
    vertex_data->GetPointData()->SetActiveVectors(attr_name.c_str());
    return vertex_data;
}
