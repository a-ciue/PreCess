/**
 * @file IgesModelHandler.cpp
 * @brief IGES 模型文件处理器实现
 * @author 范成通
 */
#include "IgesModelHandler.h"
#include "ArgType.h"
#include "ComponentData.h"
#include "GeometryData.h"
#include "IgesXdeComponentBuilder.h"
#include "ModelData.h"
#include "ModelLayer.h"

#include <BRep_Builder.hxx>
#include <IGESCAFControl_Reader.hxx>
#include <IGESControl_Writer.hxx>
#include <TDocStd_Document.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>
#include <XCAFApp_Application.hxx>
#include <spdlog/spdlog.h>

namespace systems::io {
using core::ArgType;

/**
 * @brief 读取 IGES 文件
 */
std::unique_ptr<ModelData> IgesModelHandler::read_model(const fs::path& path,
    const std::vector<std::any>& args)
{
    Handle(TDocStd_Document) doc;
    Handle(XCAFApp_Application)::DownCast(XCAFApp_Application::GetApplication())
        ->NewDocument("MDTV-XCAF", doc);

    IGESCAFControl_Reader reader;
    // 使用UTF-8编码字符串路径，配合C++17 std::filesystem处理中文路径
    IFSelect_ReturnStatus stat = reader.ReadFile(path.u8string().c_str());
    if (stat != IFSelect_RetDone) {
        spdlog::error("Failed to read IGES file: {}", path.string());
        return nullptr;
    }

    if (!reader.Transfer(doc)) {
        spdlog::error("Failed to transfer IGES file: {}", path.string());
        return nullptr;
    }

    auto model_data = IgesXdeComponentBuilder::buildModelData(*doc, path.filename().string());
    return model_data;
}

/**
 * @brief 写入 IGES 文件
 */
void IgesModelHandler::write_components(const ModelLayer& mgr,
    const std::vector<Index>& component_ids,
    const fs::path& path,
    const std::vector<std::any>& args)
{
    if (component_ids.empty()) {
        spdlog::error("IgesModelHandler: write_components called with empty component_ids");
        return;
    }

    // 收集要导出的 shape
    std::vector<TopoDS_Shape> shapes;
    shapes.reserve(component_ids.size());

    for (Index cid : component_ids) {
        const ComponentData* comp = mgr.findComponent(cid);
        if (!comp) {
            spdlog::warn("IgesModelHandler: component {} not found, skip", cid);
            continue;
        }
        if (!comp->geometry || !comp->geometry->rootShape) {
            spdlog::warn("IgesModelHandler: component {} has no geometry(rootShape), skip", cid);
            continue;
        }
        shapes.push_back(*comp->geometry->rootShape);
    }

    if (shapes.empty()) {
        spdlog::error("IgesModelHandler: no CAD shapes to export for given components");
        return;
    }

    // 多组件：合并成一个 compound 写出（与 STEP 插件保持一致）
    TopoDS_Shape shape_to_write;
    if (shapes.size() == 1) {
        shape_to_write = shapes.front();
    } else {
        BRep_Builder builder;
        TopoDS_Compound compound;
        builder.MakeCompound(compound);
        for (const auto& s : shapes) {
            builder.Add(compound, s);
        }
        shape_to_write = compound;
    }

    IGESControl_Writer writer;

    Standard_Boolean transferStatus = writer.AddShape(shape_to_write);
    if (!transferStatus) {
        spdlog::error("IgesModelHandler: Failed when transferring shape(s) to IGES writer.");
        return;
    }

    writer.ComputeModel();

    // 写入文件，使用UTF-8编码字符串路径
    Standard_Boolean writeStatus = writer.Write(path.u8string().c_str());
    if (!writeStatus) {
        spdlog::error("IgesModelHandler: Failed to write IGES file: {}", path.string());
        return;
    }

    spdlog::info("IgesModelHandler: Successfully wrote IGES file: {}", path.string());
}

/**
 * @brief 读取参数 - IGES 读取不需要额外参数，返回空列表
 */
std::vector<ArgType> IgesModelHandler::read_args_type() const
{
    return {};
}

/**
 * @brief 写入参数 - IGES 写入不需要额外参数，返回空列表
 */
std::vector<ArgType> IgesModelHandler::write_args_type() const
{
    return {};
}

} // namespace systems::io