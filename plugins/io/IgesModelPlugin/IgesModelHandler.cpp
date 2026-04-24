/**
 * @file IgesModelHandler.cpp
 * @brief IGES 模型文件处理器实现
 * @author 范成通
 */
#include "IgesModelHandler.h"
#include "ArgType.h"
#include "ModelData.h"
#include "SplineData.h"

#include <IGESControl_Reader.hxx>
#include <IGESControl_Writer.hxx>
#include <spdlog/spdlog.h>

namespace systems::io {
using core::ArgType;

/**
 * @brief 读取 IGES 文件
 */
std::unique_ptr<ModelData> IgesModelHandler::read_model(const fs::path& path,
    const std::vector<std::any>& args)
{
    IGESControl_Reader reader;
    IFSelect_ReturnStatus stat;
    // 使用UTF-8编码字符串路径，配合C++17 std::filesystem处理中文路径
    stat = reader.ReadFile(path.u8string().c_str());
    if (stat != IFSelect_RetDone) {
        spdlog::error("Failed to read IGES file: {}", path.string());
        return nullptr;
    }
    reader.TransferRoots();

    // SplineData - 和 STEP 一样保存为 BRep 边界表示
    auto spline_data = std::make_unique<SplineData>();
    spline_data->rootShape = std::make_unique<TopoDS_Shape>(reader.OneShape());

    // ModelData
    auto model_data = std::make_unique<ModelData>(std::move(spline_data));
    model_data->model_name_ = path.filename().string();

    return model_data;
}

/**
 * @brief 写入 IGES 文件
 */
void IgesModelHandler::write_model(const ModelData& data, const fs::path& path,
    const std::vector<std::any>& args)
{
    auto spline_data = data.asSplineData();
    if (!spline_data) {
        spdlog::error("IgesModelHandler only supports writing SplineData.");
        return;
    }

    IGESControl_Writer writer;

    // 将形状添加到写入器
    Standard_Boolean transferStatus = writer.AddShape(*spline_data->rootShape);

    if (!transferStatus) {
        spdlog::error("Failed when transferring shape to IGES writer.");
        return;
    }

    writer.ComputeModel();

    // 写入文件，使用UTF-8编码字符串路径
    Standard_Boolean writeStatus = writer.Write(path.u8string().c_str());

    if (!writeStatus) {
        spdlog::error("Failed to write IGES file: {}", path.string());
        return;
    }

    spdlog::info("Successfully wrote IGES file: {}", path.string());
}

/**
 * @brief 读取参数 - IGES 读取不需要额外参数，返回空列表
 */
std::vector<ArgType> IgesModelHandler::read_args_type() const
{
    return { };
}

/**
 * @brief 写入参数 - IGES 写入不需要额外参数，返回空列表
 */
std::vector<ArgType> IgesModelHandler::write_args_type() const
{
    return { };
}

} // namespace systems::io