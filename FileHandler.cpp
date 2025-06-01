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
#include <TopoDS_Shape.hxx>

std::unique_ptr<ModelData> FileHandler::read(const QUrl& path)
{
    const QString ext = QFileInfo(path.toLocalFile()).suffix().toLower();
    qDebug() << "[FileHandler] read() extension =" << ext;   // ← 新增
    if (ext == "obj" || ext == "stl" || ext == "ply")          // 你支持的网格格式
        return readMesh(path);
    else if (ext == "step" || ext == "stp" || ext == "iges" || ext == "igs")
        return readSpline(path);
    else {
        qDebug() << "read: 不支持的文件扩展名:" << ext;
        return nullptr;
    }
}

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

    return std::make_unique<ModelData>( std::move(sd) );
}

std::unique_ptr<ModelData> FileHandler::readMesh(const QUrl& mesh_path)
{
    using MeshPtr = std::unique_ptr<MeshLib::CTMesh>;

    MeshPtr rawMesh = ModelUtil::read_obj_with_groups(
            mesh_path.toLocalFile().toStdU16String());

    if (!rawMesh || rawMesh->numFaces() == 0) {
        qDebug() << "导入网格文件错误:" << mesh_path;
        return nullptr;
    }
    MeshData md{std::move(rawMesh)}; // 调 MeshData 构造完成 patch/block/group
    return std::make_unique<ModelData>(std::move(md));
}

bool FileHandler::writeMesh(ModelData* model, const QString& targetPath, const QString& renderMode, const QString& extension)
{
    if (!model || !model->isMesh()) {
        qDebug() << "writeMesh: ModelData 空，或非 Mesh 类型";
        return false;
    }

    ModelRenderMode mode{};
    if (renderMode == "Face") {
        mode = ModelRenderMode::Face;
    }
    else if (renderMode == "Block") {
        mode = ModelRenderMode::Block;
    }
    else if (renderMode == "Group") {
        mode = ModelRenderMode::Group;
    }
    else {
        qDebug() << "writeMesh: 无效的 renderMode:" << renderMode;
        return false;
    }

    std::filesystem::path mesh_path = targetPath.toStdU16String();
    model->write_mesh(mesh_path, mode, extension);
    return true;
}
