#include "AttriRenderStrategyVector.h"
#include <vtkActor.h>
#include <vtkArrowSource.h>
#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkGlyph3D.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <spdlog/spdlog.h>

namespace {
// 向量 glyph 只能使用 3 分量数组，提前拦截标量属性以避免触发 VTK OutputWindow。
bool isValidVectorArray(vtkDataArray* array, const std::string& attr_name)
{
    if (!array)
        return false;

    const int component_count = array->GetNumberOfComponents();
    if (component_count == 3)
        return true;

    spdlog::error(
        "Attribute {} has {} component(s), vector rendering requires 3 components. Use scalar mode for scalar attributes.",
        attr_name, component_count);
    return false;
}
}

void AttriRenderStrategyVector::render(
    AttributeOperator op,
    const std::string& attr_name,
    std::map<std::string, std::any> args)
{
    this->cancelActiveAttribute(op);
    // 提取glyphScale参数
    double glyph_scale = op.getMeshScale() * 0.5;
    auto it = args.find("glyph_scale");
    if (it != args.end()) {
        if (it->second.type() == typeid(double)) {
            glyph_scale = std::any_cast<double>(it->second);
        } else if (it->second.type() == typeid(float)) {
            glyph_scale = static_cast<double>(std::any_cast<float>(it->second));
        } else if (it->second.type() == typeid(int)) {
            glyph_scale = static_cast<double>(std::any_cast<int>(it->second));
        } else {
            spdlog::error("glyph_scale类型不匹配，实际为: {}", it->second.type().name());
        }
    }
    // 判断是否是顶点属性
    vtkDataArray* array = op.getFacePointData()->GetArray(attr_name.c_str());
    if (array) {
        if (!isValidVectorArray(array, attr_name))
            return;
        vtkSmartPointer<vtkPolyData> point_data = op.getPointGlyphInput(attr_name);
        createGlyph3D(op, point_data, { 1.0, 0.0, 0.0 }, glyph_scale);
        return;
    }
    // 判断是否是面属性
    array = op.getFaceCellData()->GetArray(attr_name.c_str());
    if (array) {
        if (!isValidVectorArray(array, attr_name))
            return;
        vtkSmartPointer<vtkPolyData> glyphInput = op.getFaceGlyphInput(attr_name);
        createGlyph3D(op, glyphInput, { 0.0, 0.0, 1.0 }, glyph_scale);
        return;
    }
    // 判断是否是体属性
    array = op.getSolidCellData()->GetArray(attr_name.c_str());
    if (array) {
        if (!isValidVectorArray(array, attr_name))
            return;
        vtkSmartPointer<vtkPolyData> glyphInput = op.getSolidGlyphInput(attr_name);
        createGlyph3D(op, glyphInput, { 0.0, 0.6, 0.0 }, glyph_scale);
        return;
    }

    spdlog::error("Attribute {} not found in point, face or solid data.", attr_name);
}

void AttriRenderStrategyVector::createGlyph3D(AttributeOperator& op, vtkDataSet* input, const std::array<double, 3>& color, double scale)
{
    if (!input)
        return;

    vtkNew<vtkArrowSource> arrow_source; // 箭头源
    arrow_source->SetTipResolution(16);
    arrow_source->SetTipLength(0.3);
    arrow_source->SetTipRadius(0.1);

    vtkNew<vtkGlyph3D> glyph3D; // 过滤器
    glyph3D->SetSourceConnection(arrow_source->GetOutputPort());
    glyph3D->SetInputData(input);
    glyph3D->SetScaleModeToScaleByVector();
    glyph3D->SetScaleFactor(scale);
    glyph3D->OrientOn();
    glyph3D->Update();

    vtkPolyDataMapper* mapper = op.getGlyph3DMapper();
    mapper->SetInputConnection(glyph3D->GetOutputPort());
    mapper->ScalarVisibilityOff();

    vtkActor* glyph_actor = op.getGlyph3DActor();
    glyph_actor->SetMapper(mapper);
    glyph_actor->GetProperty()->SetColor(const_cast<double*>(color.data()));
    glyph_actor->SetVisibility(1);
}
