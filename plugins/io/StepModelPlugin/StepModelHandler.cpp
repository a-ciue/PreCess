/**
 * @file StepModelHandler.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "StepModelHandler.h"
#include "ArgType.h"
#include "GeometryData.h"
#include "ModelData.h"
#include "StepXdeComponentBuilder.h"
#include "ModelLayer.h"

#include <STEPCAFControl_Reader.hxx>
#include <STEPControl_Writer.hxx>
#include <TDocStd_Document.hxx>
#include <XCAFApp_Application.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>
#include <spdlog/spdlog.h>

namespace systems::io {

std::unique_ptr<ModelData> StepModelHandler::read_model(const fs::path& path, const std::vector<std::any>& args)
{
    Handle(TDocStd_Document) doc;
    Handle(XCAFApp_Application)::DownCast(XCAFApp_Application::GetApplication())
        ->NewDocument("MDTV-XCAF", doc);

    STEPCAFControl_Reader reader;
    IFSelect_ReturnStatus stat = reader.ReadFile(path.string().c_str());
    if (stat != IFSelect_RetDone) {
        spdlog::error("Failed to read STEP file: {}", path.string());
        return nullptr;
    }

    if (!reader.Transfer(doc)) {
        spdlog::error("Failed to transfer STEP file: {}", path.string());
        return nullptr;
    }

    auto model_data = StepXdeComponentBuilder::buildModelData(*doc, path.filename().string());
    return model_data;
}

void StepModelHandler::write_components(const ModelLayer& mgr,
    const std::vector<Index>& component_ids,
    const fs::path& path,
    const std::vector<std::any>& /*args*/)
{
    if (component_ids.empty()) {
        spdlog::error("StepModelHandler: write_components called with empty component_ids");
        return;
    }

    // 收集要导出的 shape
    std::vector<TopoDS_Shape> shapes;
    shapes.reserve(component_ids.size());

    for (Index cid : component_ids) {
        const ComponentData* comp = mgr.findComponent(cid);
        if (!comp) {
            spdlog::warn("StepModelHandler: component {} not found, skip", cid);
            continue;
        }
        if (!comp->geometry || !comp->geometry->rootShape) {
            spdlog::warn("StepModelHandler: component {} has no geometry(rootShape), skip", cid);
            continue;
        }
        shapes.push_back(*comp->geometry->rootShape);
    }

    if (shapes.empty()) {
        spdlog::error("StepModelHandler: no CAD shapes to export for given components");
        return;
    }

    // 多组件：合并成一个 compound 写出（最简单策略）
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

    STEPControl_Writer writer;
    IFSelect_ReturnStatus transferStatus = writer.Transfer(shape_to_write, STEPControl_AsIs);
    if (transferStatus != IFSelect_RetDone) {
        spdlog::error("StepModelHandler: Failed when transferring shape(s) to STEP writer.");
        return;
    }

    std::ofstream of(path.string()); // 用 string 保守兼容
    if (!of) {
        spdlog::error("StepModelHandler: cannot open output file {}", path.string());
        return;
    }

    IFSelect_ReturnStatus writeStatus = writer.WriteStream(of);
    if (writeStatus != IFSelect_RetDone) {
        spdlog::error("StepModelHandler: Failed to write STEP file: {}", path.string());
        return;
    }

    spdlog::info("StepModelHandler: Successfully wrote STEP file: {}", path.string());
}

std::vector<core::ArgType> StepModelHandler::read_args_type() const
{
    return {};
}

std::vector<core::ArgType> StepModelHandler::write_args_type() const
{
    return {};
}
}
