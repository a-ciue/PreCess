/**
 * @file FileHandler.cpp
 * @brief 实现 FileHandler 类，用于管理所有与文件 IO 相关的操作
 *
 * 该文件包含 FileHandler 类的实现，提供读取样条文件、读取网格文件以及将模型数据写出到文件的功能。
 *
 * @author 徐昊阳 haoyangxu06@gmail.com
 * @date 2025/3/22
 */
#include "FileHandler.h"
#include "ModelUtil.h"
#include "Core.h"

#include <filesystem>
#include <QDebug>
#include <QFileInfo>

#include <STEPControl_Reader.hxx>
#include <IGESControl_Reader.hxx>
#include <STEPControl_Writer.hxx>
#include <TopoDS_Shape.hxx>

std::unique_ptr<ModelData> FileHandler::readSpline(const QUrl& spline_path)
{
    const QString ext = QFileInfo(spline_path.toLocalFile()).suffix().toLower();
    TopoDS_Shape shape;

    if (ext == "step" || ext == "stp") {
        STEPControl_Reader reader;
        IFSelect_ReturnStatus stat = reader.ReadFile(
                spline_path.toLocalFile().toStdString().c_str());
        if (stat != IFSelect_RetDone) {
            qDebug() << "STEP 读取失败:" << spline_path;
        return nullptr;
    }
        reader.TransferRoots();
        shape = reader.OneShape();
    }
    else {        // iges / igs
        IGESControl_Reader reader;
        IFSelect_ReturnStatus stat = reader.ReadFile(
                spline_path.toLocalFile().toStdString().c_str());
        if (stat != IFSelect_RetDone) {
            qDebug() << "IGES 读取失败:" << spline_path;
            return nullptr;
        }
        reader.TransferRoots();
        shape = reader.OneShape();
    }

    if (shape.IsNull()) {
        qDebug() << "样条文件为空:" << spline_path;
        return nullptr;
    }

    SplineData sd;
    sd.rootShape  = shape;
    sd.sourcePath = spline_path.toLocalFile();

    auto model = std::make_unique<ModelData>(std::move(sd));
    model->setModelName(spline_path.fileName());
    return model;
}

bool FileHandler::writeSpline(SplineData& spline, const std::filesystem::path& target_path)
{
	if (spline.rootShape.IsNull())
	{
		std::cerr << "writeSpline: SplineData rootShape 为空" << std::endl;
        return false;
	}

    
    if (std::filesystem::path extension = target_path.extension();
        extension == ".stp" || extension == ".step")
    {
        STEPControl_Writer writer;

	    // 将形状添加到写入器
	    IFSelect_ReturnStatus transferStatus = writer.Transfer(spline.rootShape, STEPControl_AsIs);

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
