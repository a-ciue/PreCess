#include "AttriRenderStrategyScalar.h"
#include "AttributeCommon.h"
#include <cmath>
#include <spdlog/spdlog.h>
#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkScalarBarActor.h>
#include <vtkScalarsToColors.h>

namespace {
// 移除属性类型前缀和分量后缀，只用于颜色表标题显示。
std::string scalarBarTitle(const std::string& attr_name)
{
    std::string title = attr_name;
    if (title.rfind("v_", 0) == 0 || title.rfind("e_", 0) == 0
        || title.rfind("f_", 0) == 0 || title.rfind("s_", 0) == 0) {
        title.erase(0, 2);
    }
    const size_t suffix_pos = title.find_last_of('_');
    if (suffix_pos != std::string::npos && suffix_pos + 1 < title.size()) {
        bool numeric_suffix = true;
        for (size_t i = suffix_pos + 1; i < title.size(); ++i) {
            if (title[i] < '0' || title[i] > '9') {
                numeric_suffix = false;
                break;
            }
        }
        if (numeric_suffix)
            title.erase(suffix_pos);
    }
    return title;
}
}

AttriRenderStrategyScalar::AttriRenderStrategyScalar(vtkScalarBarActor* scalar_bar)
    : scalar_bar_(scalar_bar)
{
}

void AttriRenderStrategyScalar::showScalarBar(
    vtkPolyDataMapper* mapper,
    const std::string& title,
    const double range[2])
{
    if (!scalar_bar_ || !mapper)
        return;

    vtkScalarsToColors* lookup_table = mapper->GetLookupTable();
    lookup_table->SetRange(range);
    lookup_table->Build();
    scalar_bar_->SetLookupTable(lookup_table);
    scalar_bar_->SetTitle(scalarBarTitle(title).c_str());
    scalar_bar_->SetVisibility(true);
}

void AttriRenderStrategyScalar::render(
    AttributeOperator op,
    const std::string& attr_name,
    std::map<std::string, std::any> args)
{
    // 直接调用策略也先校验映射参数，非法输入不能清除原有着色。
    std::vector<double> scalar_range;
    if (const auto it = args.find("scalar_range"); it != args.end()) {
        const auto* values = std::any_cast<std::vector<double>>(&it->second);
        if (!values || (!values->empty() && (values->size() != 2 || !std::isfinite((*values)[0]) || !std::isfinite((*values)[1]) || (*values)[0] > (*values)[1]))) {
            spdlog::warn("标量映射范围必须为空或包含两个有序有限数值");
            return;
        }
        scalar_range = *values;
    }
    // 查询、筛选与着色共用名称优先级；命中后不再向后寻找其他实体域。
    const auto attribute = findScalarAttribute(attr_name, op.getFacePointData(), op.getEdgeCellData(),
        op.getFaceCellData(), op.getSolidCellData());
    auto* array = attribute.values;
    if (!array) {
        spdlog::error("Attribute {} not found in point, edge, face or solid data.", attr_name);
        return;
    }
    this->cancelActiveAttribute(op);
    auto resolveRange = [&scalar_range](vtkDataArray* array, double range[2]) {
        // 不可用结果以 NaN 保留单元位置；全不可用时只着 NaN 色，不生成虚假色标。
        array->GetFiniteRange(range);
        if (!std::isfinite(range[0]) || !std::isfinite(range[1]) || range[0] > range[1]) {
            range[0] = 0.0;
            range[1] = 1.0;
            return false;
        }
        if (scalar_range.size() == 2) {
            range[0] = scalar_range[0];
            range[1] = scalar_range[1];
        }
        return true;
    };
    if (attribute.type == ElementType::VERTEX) {
        double range[2];
        const bool has_range = resolveRange(array, range);

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
        if (has_range)
            showScalarBar(face_mapper, attr_name, range);
        solid_mapper->SetLookupTable(face_mapper->GetLookupTable());
        return;
    }
    // 判断是否是边属性
    if (attribute.type == ElementType::EDGE) {
        double range[2];
        const bool has_range = resolveRange(array, range);

        op.getEdgeCellData()->SetActiveScalars(attr_name.c_str());
        vtkPolyDataMapper* edge_mapper = op.getEdgeMapper();
        edge_mapper->SetScalarRange(range[0], range[1]);
        edge_mapper->SetScalarModeToUseCellData();
        edge_mapper->SetScalarVisibility(1);
        edge_mapper->SetColorModeToMapScalars();
        if (has_range)
            showScalarBar(edge_mapper, attr_name, range);
        return;
    }
    // 判断是否是面属性
    if (attribute.type == ElementType::FACE) {
        op.enableFaceAttributeOffset();
        double range[2];
        const bool has_range = resolveRange(array, range);

        op.getFaceCellData()->SetActiveScalars(attr_name.c_str());
        vtkPolyDataMapper* face_mapper = op.getFaceMapper();
        face_mapper->SetScalarRange(range[0], range[1]);
        face_mapper->SetScalarModeToUseCellData();
        face_mapper->SetScalarVisibility(1);
        face_mapper->SetColorModeToMapScalars(); // 使用 colormap
        if (has_range)
            showScalarBar(face_mapper, attr_name, range);
        return;
    }
    // 判断是否是体属性
    if (attribute.type == ElementType::SOLID) {
        op.disableFaceAttributeOffset();
        double range[2];
        const bool has_range = resolveRange(array, range);

        op.getSolidCellData()->SetActiveScalars(attr_name.c_str());
        vtkPolyDataMapper* solid_mapper = op.getSolidMapper();
        solid_mapper->SetScalarRange(range[0], range[1]);
        solid_mapper->SetScalarModeToUseCellData();
        solid_mapper->SetScalarVisibility(1);
        solid_mapper->SetColorModeToMapScalars();
        if (has_range)
            showScalarBar(solid_mapper, attr_name, range);
        return;
    }
}
