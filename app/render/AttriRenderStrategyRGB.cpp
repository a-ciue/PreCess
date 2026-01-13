#include "AttriRenderStrategyRGB.h"
#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <spdlog/spdlog.h>
void AttriRenderStrategyRGB::Render(
    AttributeOperator* op,
    const std::string& attr_name,
    std::map<std::string, std::any> args)
{
    if (!op)
        return;
    this->cancelActiveAttribute(op);
    // 先判断是否是顶点属性
    vtkDataArray* array = nullptr;
    array = op->getVertexData()->GetPointData()->GetArray(attr_name.c_str());
    if (array) {
        vtkPolyData* vertex_data = op->getVertexData();
        vtkPolyDataMapper* vertex_mapper = op->getVertexMapper();
        vertex_data->GetPointData()->SetActiveScalars(attr_name.c_str());
        vertex_mapper->SetScalarVisibility(1);
        vertex_mapper->SetColorModeToDirectScalars(); // 直接映射RGB
        vertex_mapper->SetScalarModeToUsePointData();
        if (vtkPolyData* face_data = op->getFaceData()) {
            face_data->GetPointData()->SetActiveScalars(attr_name.c_str());
            vtkPolyDataMapper* face_mapper = op->getFaceMapper();
            face_mapper->SetScalarModeToUsePointData();// 面使用点数据
            face_mapper->SetScalarVisibility(1);
            face_mapper->SetColorModeToDirectScalars();
        }
        return;
    }
    // 判断是否是面属性
    if (vtkPolyData* face_data = op->getFaceData()) {
        array = face_data->GetCellData()->GetArray(attr_name.c_str());
    }
    if (array) {
        vtkPolyData* face_data = op->getFaceData();
        vtkPolyDataMapper* face_mapper = op->getFaceMapper();
        face_data->GetCellData()->SetActiveScalars(attr_name.c_str());
        face_mapper->SetScalarModeToUseCellData();
        face_mapper->SetScalarVisibility(1);
        face_mapper->SetColorModeToDirectScalars();
    } else {
        spdlog::error("Attribute {} not found in vertex or face data.", attr_name);
    }
}   