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
#include "ModelActor.h"

#include <filesystem>
#include <QDebug>

std::unique_ptr<Model> FileHandler::readSpline(const QUrl& spline_path)
{
    auto mesh = ModelUtil::mesh_from_spline(spline_path.toLocalFile().toStdU16String());
    if (!mesh || mesh->numFaces() == 0) {
        qDebug() << "导入样条文件错误:" << spline_path;
        return nullptr;
    }
    return std::make_unique<Model>(std::move(mesh));
}

std::unique_ptr<Model> FileHandler::readMesh(const QUrl& mesh_path)
{
    auto mesh = ModelUtil::read_obj_with_groups(mesh_path.toLocalFile().toStdU16String());
    if (!mesh || mesh->numFaces() == 0) {
        qDebug() << "导入网格文件错误:" << mesh_path;
        return nullptr;
    }
    return std::make_unique<Model>(std::move(mesh));
}

bool FileHandler::writeMesh(Model* model, const QString& targetPath, const QString& renderMode, const QString& extension)
{
    if (!model) {
        qDebug() << "writeMesh: Model is null";
        return false;
    }

    ModelActor::RenderMode mode{};
    if (renderMode == "Face") {
        mode = ModelActor::RenderMode::Face;
    }
    else if (renderMode == "Block") {
        mode = ModelActor::RenderMode::Block;
    }
    else if (renderMode == "Group") {
        mode = ModelActor::RenderMode::Group;
    }
    else {
        qDebug() << "writeMesh: 无效的 renderMode:" << renderMode;
        return false;
    }

    std::filesystem::path mesh_path = targetPath.toStdU16String();
    model->write_mesh(mesh_path, mode, extension);
    return true;
}
