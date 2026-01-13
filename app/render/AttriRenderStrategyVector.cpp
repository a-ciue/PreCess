#include "AttriRenderStrategyVector.h"
#include <vtkArrowSource.h>
#include <vtkCellCenters.h>
#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkGlyph3D.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkProperty.h>
#include <spdlog/spdlog.h>
void AttriRenderStrategyVector::Render(
    AttributeOperator* op,
    const std::string& attr_name,
    std::map<std::string, std::any> args)
{
    if (!op)
        return;
    this->cancelActiveAttribute(op);
    // 提取glyphScale参数
    double glyph_scale = 0.5;
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
    vtkDataArray* array = nullptr;
    array = op->getVertexData()->GetPointData()->GetArray(attr_name.c_str());
    if (array) {
        vtkPolyData* vertex_data = op->getVertexData();
        vtkDataArray* array = vertex_data->GetPointData()->GetArray(attr_name.c_str());
        assert(array);
        vertex_data->GetPointData()->SetActiveVectors(attr_name.c_str());
        createGlyph3D(op, vertex_data, { 1.0, 0.0, 0.0 }, glyph_scale); // 红色
        return;
    }
    // 判断是否是面属性
    if (vtkPolyData* face_data = op->getFaceData()) {
        array = face_data->GetCellData()->GetArray(attr_name.c_str());
    }
    if (array) {
        vtkPolyData* face_data = op->getFaceData();
        array = face_data->GetCellData()->GetArray(attr_name.c_str());
        assert(array);
        // 计算面中心点位置 =====
        vtkNew<vtkCellCenters> centers;
        centers->SetInputData(face_data);
        centers->Update();
        // 将面中心点位置与向量数据合并 =====
        vtkNew<vtkPolyData> glyphInput;
        glyphInput->SetPoints(centers->GetOutput()->GetPoints());
        glyphInput->GetPointData()->SetVectors(array);
        createGlyph3D(op, glyphInput, { 0.0, 0.0, 1.0 }, glyph_scale); // 蓝色
    } else {
        spdlog::error("Attribute {} not found in vertex or face data.", attr_name);
    }
}
void AttriRenderStrategyVector::createGlyph3D(AttributeOperator* op, vtkDataSet* input, const std::array<double, 3>& color, double scale)
{
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

    vtkPolyDataMapper* mapper = op->getGlyph3DMapper();
    mapper->SetInputConnection(glyph3D->GetOutputPort());
    mapper->ScalarVisibilityOff();

    vtkActor* glyph_actor = op->getGlyph3DActor();
    glyph_actor->SetMapper(mapper);
    glyph_actor->GetProperty()->SetColor(const_cast<double*>(color.data()));
    glyph_actor->SetVisibility(1);
}
