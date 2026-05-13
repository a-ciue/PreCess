/**
 * @file StepModelHandler.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "StepModelHandler.h"
#include "ArgType.h"
#include "SplineData.h"
#include "ModelData.h"

#include <STEPControl_Reader.hxx>
#include <STEPControl_Writer.hxx>
#include <spdlog/spdlog.h>

namespace systems::io {
std::unique_ptr<ModelData> StepModelHandler::read_model(const fs::path& path, const std::vector<std::any>& args)
{
    STEPControl_Reader reader;
    IFSelect_ReturnStatus stat;
    // 使用UTF-8编码字符串路径，配合C++17 std::filesystem处理中文路径
    stat = reader.ReadFile(path.u8string().c_str());
    if (stat != IFSelect_RetDone) {
        spdlog::error("Failed to read STEP file: {}", path.string());
        return nullptr;
    }
    reader.TransferRoots();

    // SplineData
    auto spline_data = std::make_unique<SplineData>();
    spline_data->rootShape = std::make_unique<TopoDS_Shape>(reader.OneShape());

    // ModelData
    auto model_data = std::make_unique<ModelData>(std::move(spline_data));
    model_data->model_name_ = path.filename().string();

    return model_data;
}

void StepModelHandler::write_model(const ModelData& data, const fs::path& path, const std::vector<std::any>& args)
{
    auto spline_data = data.asSplineData();
    if (!spline_data) {
        spdlog::error("StepModelHandler only supports writing SplineData.");
    }

    STEPControl_Writer writer;

    // 将形状添加到写入器
    IFSelect_ReturnStatus transferStatus = writer.Transfer(*spline_data->rootShape, STEPControl_AsIs);

    if (transferStatus != IFSelect_RetDone) {
        spdlog::error("Failed when transferring shape to STEP writer.");
        return;
    }

    // 写入文件
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
