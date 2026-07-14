#include "AttriRenderStrategyScalar.h"
#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <spdlog/spdlog.h>
void AttriRenderStrategyScalar::render(
    AttributeOperator op,
    const std::string& attr_name,
    std::map<std::string, std::any> args)
{
    this->cancelActiveAttribute(op);
    // 提取scalar_range参数
    std::vector<double> scalar_range;
    auto it = args.find("scalar_range");
    if (it != args.end() ) {
        const auto& vec = std::any_cast<const std::vector<double>&>(it->second);
        if (vec.size() == 2) {
            scalar_range = vec;
        }
    }

    auto resolveRange = [&scalar_range](vtkDataArray* array, double range[2]) {
        if (scalar_range.size() == 2) {
            range[0] = scalar_range[0];
            range[1] = scalar_range[1];
        } else {
            array->GetRange(range);
        }
    };
    // 判断是否是点属性
    vtkDataArray* array = op.getFacePointData()->GetArray(attr_name.c_str());
    if (array) {
        double range[2];
        resolveRange(array, range);

        op.getFacePointData()->SetActiveScalars(attr_name.c_str());
        op.getSolidPointData()->SetActiveScalars(attr_name.c_str());

        vtkPolyDataMapper* face_mapper = op.getFaceMapper();
        face_mapper->SetScalarModeToUsePointData();
        face_mapper->SetScalarVisibility(1);
        face_mapper->SetScalarRange(range[0], range[1]);
        face_mapper->SetColorModeToMapScalars();

        vtkPolyDataMapper* solid_mapper = op.getSolidMapper();
        solid_mapper->SetScalarModeToUsePointData();
        solid_mapper->SetScalarVisibility(1);
        solid_mapper->SetScalarRange(range[0], range[1]);
        solid_mapper->SetColorModeToMapScalars();
        op.showScalarBar(face_mapper, attr_name, range);
        solid_mapper->SetLookupTable(face_mapper->GetLookupTable());
        return;
    }
    // 判断是否是面属性
    array = op.getFaceCellData()->GetArray(attr_name.c_str());
    if (array) {
        op.enableFaceAttributeOffset();
        double range[2];
        resolveRange(array, range);

        op.getFaceCellData()->SetActiveScalars(attr_name.c_str());
        vtkPolyDataMapper* face_mapper = op.getFaceMapper();
        face_mapper->SetScalarRange(range[0], range[1]);
        face_mapper->SetScalarModeToUseCellData();
        face_mapper->SetScalarVisibility(1);
        face_mapper->SetColorModeToMapScalars(); // 使用 colormap
        op.showScalarBar(face_mapper, attr_name, range);
        return;
    }
    // 判断是否是体属性
    array = op.getSolidCellData()->GetArray(attr_name.c_str());
    if (array) {
        op.disableFaceAttributeOffset();
        double range[2];
        resolveRange(array, range);

        op.getSolidCellData()->SetActiveScalars(attr_name.c_str());
        vtkPolyDataMapper* solid_mapper = op.getSolidMapper();
        solid_mapper->SetScalarRange(range[0], range[1]);
        solid_mapper->SetScalarModeToUseCellData();
        solid_mapper->SetScalarVisibility(1);
        solid_mapper->SetColorModeToMapScalars();
        op.showScalarBar(solid_mapper, attr_name, range);
        return;
    }

    spdlog::error("Attribute {} not found in point, face or solid data.", attr_name);
}