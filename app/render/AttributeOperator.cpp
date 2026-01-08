#include "AttributeOperator.h"
#include "MeshActor.h"
#include <array>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <vtkActor.h>
#include <vtkAlgorithmOutput.h>
#include <vtkArrowSource.h>
#include <vtkCellCenters.h>
#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkDataSet.h>
#include <vtkDataSetAttributes.h>
#include <vtkGlyph3D.h>
#include <vtkImageReader2.h>
#include <vtkImageReader2Factory.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkSmartPointer.h>
#include <vtkTexture.h>
#include <filesystem>

AttributeOperator::AttributeOperator(MeshActor* actor)
    : actor_(actor)
{
}

void AttributeOperator::setAttriMode(
    const std::string& attr_name,
    Mode mode,
    ElementType type,
    const std::string& texture_path,
    double glyph_scale,
    std::optional<std::pair<double, double>> scalar_range)
{
    cancelActiveAttribute();
    spdlog::info("Mode:{} type:{}", static_cast<int>(mode), static_cast<int>(type));
    switch (mode) {
    case Mode::SCALAR:
        setActiveScalarAttribute(attr_name, type);
        if (scalar_range.has_value()) {
            setScalarRange(scalar_range.value().first, scalar_range.value().second);
        }
        break;
    case Mode::VECTOR:
        setActiveVectorAttribute(attr_name, type);
        if (glyph_scale > 0)
            setGlyph3DScaleFactor(glyph_scale);
        break;
    case Mode::RGB:
        setActiveRGBAttribute(attr_name, type);
        break;
    case Mode::UV:
        setTextureImage(attr_name, texture_path);
        break;
    default:
        spdlog::error("not the defalt mode");
        assert(false);
        break;
    }
}

void AttributeOperator::cancelActiveAttribute()
{
    if (actor_->face_actor_->GetTexture() != nullptr) {
        cancelTextureImage();
    }
    actor_->glyph3D_actor_->SetVisibility(0);
    actor_->vertex_mapper_->SetScalarVisibility(0);
    actor_->edge_mapper_->SetScalarVisibility(0);
    actor_->face_mapper_->SetScalarVisibility(0);
    actor_->solid_mapper_->SetScalarVisibility(0);
}

void AttributeOperator::setActiveScalarAttribute(std::string attr_name, ElementType type)
{
    switch (type) {
    case ElementType::VERTEX: {
        vtkDataArray* array = actor_->vertex_data_->GetPointData()->GetArray(attr_name.c_str());
        assert(array);
        actor_->vertex_data_->GetPointData()->SetActiveAttribute(attr_name.c_str(), vtkDataSetAttributes::SCALARS);
        // 设置映射范围
        double range[2];
        array->GetRange(range);
        actor_->vertex_mapper_->SetScalarRange(range[0], range[1]);
        actor_->vertex_mapper_->SetScalarVisibility(1);
        actor_->vertex_mapper_->SetColorModeToMapScalars(); // 使用 colormap
        if (actor_->face_data_) {
            actor_->face_data_->GetPointData()->SetActiveScalars(attr_name.c_str());
            actor_->face_mapper_->SetScalarModeToUsePointData();
            actor_->face_mapper_->SetScalarVisibility(1);
            actor_->face_mapper_->SetScalarRange(range[0], range[1]);
            actor_->face_mapper_->SetColorModeToMapScalars();
        }
        break;
    }
    case ElementType::FACE: {
        vtkDataArray* array = actor_->face_data_->GetCellData()->GetArray(attr_name.c_str());
        assert(array);
        // 设置映射范围
        double range[2];
        array->GetRange(range);
        actor_->face_mapper_->SetScalarRange(range[0], range[1]);
        actor_->face_mapper_->SetScalarModeToUseCellData();
        actor_->face_data_->GetCellData()->SetActiveScalars(attr_name.c_str());
        actor_->face_mapper_->SetScalarVisibility(1);
        actor_->face_mapper_->SetColorModeToMapScalars(); // 使用 colormap
        break;
    }
    case ElementType::EDGE:
        break;
    }
}

void AttributeOperator::setActiveVectorAttribute(std::string attr_name, ElementType type)
{
    switch (type) {
    case ElementType::VERTEX: {
        vtkDataArray* array = actor_->vertex_data_->GetPointData()->GetArray(attr_name.c_str());
        assert(array);
        actor_->vertex_data_->GetPointData()->SetActiveVectors(attr_name.c_str());
        createGlyph3D(actor_->vertex_data_, { 1.0, 0.0, 0.0 }); // 红色
        break;
    }
    case ElementType::FACE: {
        vtkDataArray* array = actor_->face_data_->GetCellData()->GetArray(attr_name.c_str());
        assert(array);
        // 计算面中心点位置 =====
        vtkNew<vtkCellCenters> centers;
        centers->SetInputData(actor_->face_data_);
        centers->Update();
        // 将面中心点位置与向量数据合并 =====
        vtkNew<vtkPolyData> glyphInput;
        glyphInput->SetPoints(centers->GetOutput()->GetPoints());
        glyphInput->GetPointData()->SetVectors(array);
        createGlyph3D(glyphInput, { 0.0, 0.0, 1.0 }); // 蓝色

        break;
    }
    case ElementType::EDGE:
        break;
    }
}

void AttributeOperator::setActiveRGBAttribute(std::string attr_name, ElementType type)
{
    switch (type) {
    case ElementType::VERTEX: {
        actor_->vertex_data_->GetPointData()->SetActiveAttribute(attr_name.c_str(), vtkDataSetAttributes::SCALARS);
        actor_->vertex_mapper_->SetScalarVisibility(1);
        actor_->vertex_mapper_->SetColorModeToDirectScalars(); // 使用 RGB
        actor_->face_data_->GetPointData()->SetActiveScalars(attr_name.c_str());
        actor_->face_mapper_->SetScalarModeToUsePointData(); // 面使用点数据的插值
        actor_->face_mapper_->SetScalarVisibility(1);
        actor_->face_mapper_->SetColorModeToDirectScalars();
        break;
    }
    case ElementType::FACE: {
        actor_->face_mapper_->SetScalarModeToUseCellData();
        actor_->face_data_->GetCellData()->SetActiveScalars(attr_name.c_str());
        actor_->face_mapper_->SetScalarVisibility(1);
        actor_->face_mapper_->SetColorModeToDirectScalars();
        break;
    }
    case ElementType::EDGE:
        break;
    }
}

void AttributeOperator::setTextureImage(std::string attr_name, std::string texturePath)
{
    if (!std::filesystem::exists(texturePath)) {
        spdlog::error("Texture file not found: {}", texturePath);
        return;
    }
    spdlog::info("start setTextureImage---------------------------");

    // 读取纹理贴图文件
    vtkNew<vtkImageReader2Factory> reader_factory;
    vtkSmartPointer<vtkImageReader2> texture_file = reader_factory->CreateImageReader2(texturePath.c_str());
    if (!texture_file) {
        spdlog::error("Error: Failed to create texture reader for {}", texturePath);
        return;
    }
    texture_file->SetFileName(texturePath.c_str());
    texture_file->Update();

    // 创建纹理对象
    vtkNew<vtkTexture> texture;
    texture->SetInputConnection(texture_file->GetOutputPort());
    texture->InterpolateOn(); // 启用插值使纹理更平滑

    // 读取传入的属性名作为UV
    vtkDataArray* tcoords = actor_->vertex_data_->GetPointData()->GetArray(attr_name.c_str());
    if (tcoords && tcoords->GetNumberOfComponents() == 2) {
        spdlog::info("use attribute [{}] as UV", attr_name);
        actor_->vertex_data_->GetPointData()->SetTCoords(tcoords);
        actor_->face_data_->GetPointData()->SetTCoords(tcoords);
    }
    actor_->face_actor_->SetTexture(texture);
}

void AttributeOperator::cancelTextureImage()
{
    actor_->face_actor_->SetTexture(nullptr);
}

// Glyph3D 的缩放因子调整接口
void AttributeOperator::setGlyph3DScaleFactor(double scale)
{
    vtkMapper* mapper = actor_->glyph3D_mapper_;
    if (!mapper)
        return;
    vtkAlgorithm* producer = nullptr;
    producer = mapper->GetInputConnection(0, 0)->GetProducer();
    if (producer) {
        vtkGlyph3D* glyph = vtkGlyph3D::SafeDownCast(producer);
        glyph->SetScaleFactor(scale);
        glyph->Update();
    }
}

// 标量的range映射标调整接口
void AttributeOperator::setScalarRange(double min, double max)
{
    vtkDataArray* array = nullptr;
    vtkPolyDataMapper* mapper = nullptr;

    // 如果点映射可见且有标量，设置点，同时同步设置面（点对面插值）
    if (actor_->vertex_mapper_->GetScalarVisibility() && actor_->vertex_data_ && actor_->vertex_data_->GetPointData()->GetScalars()) {
        array = actor_->vertex_data_->GetPointData()->GetScalars();
        mapper = actor_->vertex_mapper_;
        mapper->SetScalarRange(min, max);

        // 同步设置面（点对面插值时，面mapper用点数据）
        if (actor_->face_mapper_ && actor_->face_data_ && actor_->face_data_->GetPointData()->GetScalars()) {
            actor_->face_mapper_->SetScalarModeToUsePointData();
            actor_->face_mapper_->SetScalarRange(min, max);
        }
    } else // 否则面映射可见且有标量，设置面
        if (actor_->face_mapper_->GetScalarVisibility() && actor_->face_data_ && actor_->face_data_->GetCellData()->GetScalars()) {
            array = actor_->face_data_->GetCellData()->GetScalars();
            mapper = actor_->face_mapper_;
            mapper->SetScalarRange(min, max);
        }
    return;
}

void AttributeOperator::createGlyph3D(vtkDataSet* input, const std::array<double, 3>& color, double scale)
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

    vtkPolyDataMapper* mapper = actor_->glyph3D_mapper_;
    mapper->SetInputConnection(glyph3D->GetOutputPort());
    mapper->ScalarVisibilityOff();

    vtkActor* actor = actor_->glyph3D_actor_;
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(const_cast<double*>(color.data()));
    actor->SetVisibility(1);
}