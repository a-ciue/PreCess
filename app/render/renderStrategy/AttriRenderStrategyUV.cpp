#include "AttriRenderStrategyUV.h"
#include <filesystem>
#include <spdlog/spdlog.h>
#include <vtkDataArray.h>
#include <vtkImageReader2Factory.h>
#include <vtkPNGReader.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkTexture.h>
void AttriRenderStrategyUV::render(
    AttributeOperator op,
    const std::string& attr_name,
    std::map<std::string, std::any> args)
{
    this->cancelActiveAttribute(op);
    // 获取贴图路径
    std::string texture_path;
    auto it = args.find("texture_path");
    if (it != args.end()) {
        texture_path = std::any_cast<std::string>(it->second);
        if (!std::filesystem::exists(texture_path)) {
            spdlog::error("Texture file not found: {}", texture_path);
            return;
        }
    }

    // 读取纹理贴图文件
    vtkNew<vtkImageReader2Factory> reader_factory;
    vtkSmartPointer<vtkImageReader2> texture_file = reader_factory->CreateImageReader2(texture_path.c_str());
    if (!texture_file) {
        spdlog::error("Error: Failed to create texture reader for {}", texture_path);
        return;
    }
    texture_file->SetFileName(texture_path.c_str());
    texture_file->Update();

    // 创建纹理对象
    vtkNew<vtkTexture> texture;
    texture->SetInputConnection(texture_file->GetOutputPort());
    texture->InterpolateOn(); // 启用插值使纹理更平滑

    // 读取传入的属性作为UV
    vtkDataArray* tcoords = op.getVertexPointData()->GetArray(attr_name.c_str());
    if (tcoords && tcoords->GetNumberOfComponents() == 2) {
        spdlog::info("use attribute {} as UV", attr_name);
        op.getVertexPointData()->SetTCoords(tcoords);
        op.getFacePointData()->SetTCoords(tcoords);
    }
    op.getFaceActor()->SetTexture(texture);
}