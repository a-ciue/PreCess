/**
 * @file FileHandler.cpp
 * @brief 实现 FileHandler 类，用于管理所有与文件 IO 相关的操作
 *
 * 该文件包含 FileHandler 类的实现，提供读取几何文件、读取网格文件以及将模型数据写出到文件的功能。
 *
 * @author 徐昊阳 haoyangxu06@gmail.com
 * @date 2025/3/22
 */
#include "FileHandler.h"
#include "Core.h"
#include "GeometryData.h"

#include <filesystem>

#include <STEPControl_Reader.hxx>
#include <IGESControl_Reader.hxx>
#include <STEPControl_Writer.hxx>
#include <TopoDS_Shape.hxx>
#include <spdlog/spdlog.h>

std::unique_ptr<ModelData> FileHandler::readGeometry(const std::filesystem::path& geometry_path)
{
    using namespace std::filesystem;
    //const QString ext = QFileInfo(geometry_path.toLocalFile()).suffix().toLower();
    const path& ext = geometry_path.extension();
    TopoDS_Shape shape;

    if (ext == "step" || ext == "stp") {
        STEPControl_Reader reader;
        IFSelect_ReturnStatus stat = reader.ReadFile(
                geometry_path.string().c_str());
        if (stat != IFSelect_RetDone) {
            spdlog::error("STEP 读取失败: {}", geometry_path.string());
        return nullptr;
    }
        reader.TransferRoots();
        shape = reader.OneShape();
    }
    else {        // iges / igs
        IGESControl_Reader reader;
        IFSelect_ReturnStatus stat = reader.ReadFile(
            geometry_path.string().c_str());
        if (stat != IFSelect_RetDone) {
            spdlog::error("IGES 读取失败: {}", geometry_path.string());
            return nullptr;
        }
        reader.TransferRoots();
        shape = reader.OneShape();
    }

    if (shape.IsNull()) {
        spdlog::error("几何文件 {} 读取失败，形状为空", geometry_path.string());
        return nullptr;
    }

    auto sd = std::make_unique<GeometryData>();
    sd->rootShape = std::make_unique<TopoDS_Shape>(shape);

    auto model = std::make_unique<ModelData>(std::move(sd));
    model->model_name_ = geometry_path.filename().string();
    return model;
}

bool FileHandler::writeGeometry(GeometryData& geometry, const std::filesystem::path& target_path)
{
	if (!geometry.rootShape || geometry.rootShape->IsNull())
	{
		std::cerr << "writeGeometry: GeometryData rootShape 为空" << std::endl;
        return false;
	}

    
    if (std::filesystem::path extension = target_path.extension();
        extension == ".stp" || extension == ".step")
    {
        STEPControl_Writer writer;

	    // 将形状添加到写入器
	    IFSelect_ReturnStatus transferStatus = writer.Transfer(*geometry.rootShape, STEPControl_AsIs);

	    if (transferStatus != IFSelect_RetDone) {
	        std::cerr << "形状传输失败" << std::endl;
	        return false;
	    }

	    // 写入文件
	    std::ofstream of(target_path);
	    IFSelect_ReturnStatus writeStatus = writer.WriteStream(of);

	    if (writeStatus != IFSelect_RetDone) {
	        std::cerr << "写入 STEP 文件失败" << std::endl;
	        return false;
	    }

        return true;
    }
    std::cerr << "未匹配拓展名" << std::endl;
    return false;
}
