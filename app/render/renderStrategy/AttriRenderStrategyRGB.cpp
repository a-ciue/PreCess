#include "AttriRenderStrategyRGB.h"
#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <spdlog/spdlog.h>
void AttriRenderStrategyRGB::render(
    AttributeOperator op,
    const std::string& attr_name,
    std::map<std::string, std::any> args)
{
    this->cancelActiveAttribute(op);
    // 判断是否是点属性
    vtkDataArray* array = op.getFacePointData()->GetArray(attr_name.c_str());
    if (array) {
        op.getFacePointData()->SetActiveScalars(attr_name.c_str());
        op.getSolidPointData()->SetActiveScalars(attr_name.c_str());

        vtkPolyDataMapper* face_mapper = op.getFaceMapper();
        face_mapper->SetScalarModeToUsePointData();// 面使用点数据
        face_mapper->SetScalarVisibility(1);
        face_mapper->SetColorModeToDirectScalars();

        vtkPolyDataMapper* solid_mapper = op.getSolidMapper();
        solid_mapper->SetScalarModeToUsePointData();
        solid_mapper->SetScalarVisibility(1);
        solid_mapper->SetColorModeToDirectScalars();
        return;
    }
    // 判断是否是面属性
    array = op.getFaceCellData()->GetArray(attr_name.c_str());
    if (array) {
        op.getFaceCellData()->SetActiveScalars(attr_name.c_str());
        vtkPolyDataMapper* face_mapper = op.getFaceMapper();
        face_mapper->SetScalarModeToUseCellData();
        face_mapper->SetScalarVisibility(1);
        face_mapper->SetColorModeToDirectScalars();
        return;
    }
    // 判断是否是体属性
    array = op.getSolidCellData()->GetArray(attr_name.c_str());
    if (array) {
        op.getSolidCellData()->SetActiveScalars(attr_name.c_str());
        vtkPolyDataMapper* solid_mapper = op.getSolidMapper();
        solid_mapper->SetScalarModeToUseCellData();
        solid_mapper->SetScalarVisibility(1);
        solid_mapper->SetColorModeToDirectScalars();
        return;
    }

    spdlog::error("Attribute {} not found in point, face or solid data.", attr_name);
}