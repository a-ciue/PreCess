/**
 * @file StepModelHandler.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "StepModelHandler.h"
#include "ArgType.h"
#include "SplineData.h"
#include "ModelData.h"
#include "StepXdeComponentBuilder.h"

#include <STEPCAFControl_Reader.hxx>
#include <STEPControl_Writer.hxx>
#include <TDocStd_Document.hxx>
#include <XCAFApp_Application.hxx>
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

void StepModelHandler::write_model(const ModelData& data, const fs::path& path, const std::vector<std::any>& args)
{
    auto spline_data = data.asSplineData();
    if (!spline_data) {
        spdlog::error("StepModelHandler only supports writing SplineData.");
    }

    STEPControl_Writer writer;

    IFSelect_ReturnStatus transferStatus = writer.Transfer(*spline_data->rootShape, STEPControl_AsIs);

    if (transferStatus != IFSelect_RetDone) {
        spdlog::error("Failed when transferring shape to STEP writer.");
        return;
    }

    std::ofstream of(path);
    IFSelect_ReturnStatus writeStatus = writer.WriteStream(of);

    if (writeStatus != IFSelect_RetDone) {
        spdlog::error("Failed to write STEP file: {}", path.string());
        return;
    }

    spdlog::info("Successfully wrote STEP file: {}", path.string());
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
