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

void ModelManager::setVtkItem(const QString& modelName, MyVtkItem* item)
{
    vtk_item_ = item;
    connectVtk(modelName);
}

// 添加模型
void ModelManager::addModel(const QString& modelName, std::unique_ptr<Model> model) {
    if (models_.find(modelName) != models_.end()) {
        throw std::runtime_error("Model with the given name already exists.");
    }
    models_[modelName] = std::move(model);
}

// 删除模型
void ModelManager::removeModel(const QString& modelName) {
    auto it = models_.find(modelName);
    if (it == models_.end()) {
        throw std::runtime_error("Model with the given name does not exist.");
    }
    models_.erase(it);
}

// 获取模型
Model* ModelManager::getModel(const QString& modelName) const {
    auto it = models_.find(modelName);
    if (it == models_.end()) {
        return nullptr; // 模型不存在时返回空指针
    }
    return it->second.get();
}


void ModelManager::readSpline(const QString& modelName, QUrl spline_path)
{
    auto model = getModel(modelName);
    if (!model) {
        qDebug() << "未找到指定的模型: " << modelName;
        return;
    }

    auto mesh = ModelUtil::mesh_from_spline(spline_path.toLocalFile().toStdU16String());
    if (!mesh || mesh->numFaces() == 0) {
        //emit splineLoadFailed(tr("fail to load spline file."));
        qDebug() << "导入文件错误: " << spline_path;
    }

    // 重新分配 std::unique_ptr<Model>，并更新模型数据
    models_[modelName] = std::make_unique<Model>(std::move(mesh));
    // 使用 get() 获取裸指针，并调用相应方法
    Model* rawModel = models_[modelName].get();
    connectVtk(modelName);
    rawModel->refreshVtk();
}

void ModelManager::readMesh(const QString& modelName, QUrl target_mesh)
{
    auto model = getModel(modelName);
    if (!model) {
        qDebug() << "未找到指定的模型: " << modelName;
        return;
    }

    auto mesh = ModelUtil::read_obj_with_groups(target_mesh.toLocalFile().toStdU16String());

    if (!mesh || mesh->numFaces() == 0) {
        //emit splineLoadFailed(tr("fail to load spline file."));
        qDebug() << "导入文件错误: " << target_mesh;
    }

    // 重新分配 std::unique_ptr<Model>，并更新模型数据
    models_[modelName] = std::make_unique<Model>(std::move(mesh));
    // 使用 get() 获取裸指针，并调用相应方法
    Model* rawModel = models_[modelName].get();
    connectVtk(modelName);
    rawModel->refreshVtk();
}

void ModelManager::writeMesh(const QString& modelName, QUrl target_mesh, QString renderMode, QString extension)
{
    auto model = getModel(modelName);
    if (!model) {
        qDebug() << "未找到指定的模型: " << modelName;
        return;
    }

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
    model->write_mesh(mesh_path, mode, extension);
}

void ModelManager::connectVtk(const QString& modelName)
{
    auto model = getModel(modelName);
    if (!model || !vtk_item_) {
        qDebug() << "模型或 VTK 项不存在: " << modelName;
        return;
    }

    connect(model, &Model::patchUpdated, vtk_item_, &MyVtkItem::patchUpdated);
    connect(model, &Model::blockUpdated, vtk_item_, &MyVtkItem::blockUpdated);
    connect(model, &Model::blocksMerged, vtk_item_, &MyVtkItem::blocksMerged);
    connect(model, &Model::groupUpdated, vtk_item_, &MyVtkItem::groupUpdated);
    connect(model, &Model::groupMerged, vtk_item_, &MyVtkItem::groupMerged);
    connect(model, &Model::modelInited, vtk_item_, &MyVtkItem::onModelInited);

}

