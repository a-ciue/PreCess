#include "ModelManager.h"
#include "ModelUtil.h"
#include "ToolMesh.h"

#include "MyVtkItem.h"
#include <QObject>
#include <filesystem>

MyVtkItem* ModelManager::vtkItem()
{
    return vtk_item_;
}

void ModelManager::setVtkItem(MyVtkItem* item)
{
    vtk_item_ = item;
    connectVtk();
}

void ModelManager::readSpline(QUrl spline_path, double size)
{
    auto mesh = ModelUtil::mesh_from_spline(spline_path.toLocalFile().toStdU16String(), size);
    if (!mesh || mesh->numFaces() == 0) {
        //emit splineLoadFailed(tr("fail to load spline file."));
        qDebug() << "导入文件错误: " << spline_path;
    }

    model_ = std::make_unique<Model>(std::move(mesh));
    connectVtk();
    model_->refreshVtk();
}

void ModelManager::readMesh(QUrl target_mesh)
{
    auto mesh = ModelUtil::read_obj_with_groups(target_mesh.toLocalFile().toStdU16String());

    if (!mesh || mesh->numFaces() == 0) {
        //emit splineLoadFailed(tr("fail to load spline file."));
        qDebug() << "导入文件错误: " << target_mesh;
    }

    model_ = std::make_unique<Model>(std::move(mesh));
    connectVtk();
    model_->refreshVtk();
}

void ModelManager::writeMesh(QUrl target_mesh, QString renderMode, QString extension)
{
    ModelActor::RenderMode mode {};
    if (renderMode == "Face") {
        mode = ModelActor::RenderMode::Face;
    } else if (renderMode == "Block") {
        mode = ModelActor::RenderMode::Block;
    } else if (renderMode == "Group") {
        mode = ModelActor::RenderMode::Group;
    } else {
        std::cerr << "invalid renderMode in MyVtkItem::changeEdgeRenderer" << std::endl;
        return;
    }

    std::filesystem::path mesh_path = target_mesh.toLocalFile().toStdU16String();
    model_->write_mesh(mesh_path, mode, extension);
}

void ModelManager::connectVtk()
{
    if (model_ && vtk_item_)
    {
        connect(model_.get(), &Model::patchUpdated, vtk_item_, &MyVtkItem::patchUpdated);
        connect(model_.get(), &Model::blockUpdated, vtk_item_, &MyVtkItem::blockUpdated);
        connect(model_.get(), &Model::blocksMerged, vtk_item_, &MyVtkItem::blocksMerged);
        connect(model_.get(), &Model::groupUpdated, vtk_item_, &MyVtkItem::groupUpdated);
        connect(model_.get(), &Model::groupMerged, vtk_item_, &MyVtkItem::groupMerged);
        connect(model_.get(), &Model::modelInited, vtk_item_, &MyVtkItem::onModelInited);
    }
}

