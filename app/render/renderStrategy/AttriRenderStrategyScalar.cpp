#include "AttriRenderStrategyScalar.h"
#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <spdlog/spdlog.h>
void AttriRenderStrategyScalar::render(
    AttributeOperator& op,
    const std::string& attr_name,
    std::map<std::string, std::any> args)
{
    this->cancelActiveAttribute(op);
    // 提取scalar_range参数
    std::optional<std::pair<double, double>> scalar_range;
    auto it = args.find("scalar_range");
    if (it != args.end()) {
        scalar_range = std::any_cast<std::pair<double, double>>(it->second);
    }

    // 先判断是否是顶点属性
    vtkDataArray* array = nullptr;
    array = op.getVertexPointData()->GetArray(attr_name.c_str());
    if (array) {
        vtkPointData* vertex_data = op.getVertexPointData();
        vtkPolyDataMapper* vertex_mapper = op.getVertexMapper();
        vertex_data->SetActiveAttribute(attr_name.c_str(), vtkDataSetAttributes::SCALARS);
        // 设置映射范围
        double range[2];
        if (scalar_range) {
            range[0] = scalar_range.value().first;
            range[1] = scalar_range.value().second;
        } else {
            array->GetRange(range);
        }
        vertex_mapper->SetScalarRange(range[0], range[1]);
        vertex_mapper->SetScalarVisibility(1);
        vertex_mapper->SetColorModeToMapScalars(); // 使用 colormap
        if (vtkPointData* face_data = op.getFacePointData()) {
            face_data->SetActiveScalars(attr_name.c_str());
            vtkPolyDataMapper* face_mapper = op.getFaceMapper();
            face_mapper->SetScalarModeToUsePointData();
            face_mapper->SetScalarVisibility(1);
            face_mapper->SetScalarRange(range[0], range[1]);
            face_mapper->SetColorModeToMapScalars();
        }
        return;
    }
    // 判断是否是面属性
    if (vtkCellData* face_data = op.getFaceCellData()) {
        array = face_data->GetArray(attr_name.c_str());
    }
    if (array) {
        vtkCellData* face_data = op.getFaceCellData();
        vtkPolyDataMapper* face_mapper = op.getFaceMapper();
        array = face_data->GetArray(attr_name.c_str());
        assert(array);
        // 设置映射范围
        double range[2];
        if (scalar_range) {
            range[0] = scalar_range.value().first;
            range[1] = scalar_range.value().second;
        } else {
            array->GetRange(range);
        }
        face_data->SetActiveScalars(attr_name.c_str());
        face_mapper->SetScalarRange(range[0], range[1]);
        face_mapper->SetScalarModeToUseCellData();
        face_mapper->SetScalarVisibility(1);
        face_mapper->SetColorModeToMapScalars(); // 使用 colormap
    } else {
        spdlog::error("Attribute {} not found in vertex or face data.", attr_name);
    }
}