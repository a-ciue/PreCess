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
    
}

// 添加模型
//void ModelManager::addModel(const QString& modelName, std::unique_ptr<Model> model) {
//    if (models_.find(modelName) != models_.end()) {
//        throw std::runtime_error("Model with the given name already exists.");
//    }
//    models_[modelName] = std::move(model);
//}
void ModelManager::addModel(const QString& modelName, std::unique_ptr<Model> model)
{
    if (models_.find(modelName) != models_.end()) {
        throw std::runtime_error("Model with the given name already exists.");
    }

    // 为模型设置名称
    model->setModelName(modelName);
    models_[modelName] = std::move(model);

    Model* rawModel = models_[modelName].get();
    if (!rawModel || !vtk_item_) {
        qDebug() << "模型或 VTK 项不存在:" << modelName;
        return;
    }

    // 使用 SIGNAL/SLOT 机制连接 Model 的信号到 MyVtkItem 的槽（这里信号参数均已增加 modelName）
    connect(rawModel, &Model::patchUpdated,
        vtk_item_, &MyVtkItem::patchUpdated);

    connect(rawModel, &Model::blockUpdated,
        vtk_item_, &MyVtkItem::blockUpdated);

    connect(rawModel, &Model::blocksMerged,
        vtk_item_, &MyVtkItem::blocksMerged);

    connect(rawModel, &Model::groupUpdated,
        vtk_item_, &MyVtkItem::groupUpdated);

    connect(rawModel, &Model::groupMerged,
        vtk_item_, &MyVtkItem::groupMerged);

    connect(rawModel, &Model::modelInited,
        vtk_item_, &MyVtkItem::onModelInited);

    // 调用模型刷新接口，确保 VTK 数据更新
    rawModel->refreshVtk();

    // 发射添加模型的信号（需要在 ModelManager.h 中声明信号 modelAdded(const QString&)）
    emit modelAdded(modelName);
}

// 删除模型
void ModelManager::removeModel(const QString& modelName) {
    auto it = models_.find(modelName);
    if (it == models_.end()) {
        throw std::runtime_error("Model with the given name does not exist.");
    }
    models_.erase(it);
    // 发射删除模型信号
    emit modelRemoved(modelName);
}

// 获取模型
Model* ModelManager::getModel(const QString& modelName) const {
    auto it = models_.find(modelName);
    if (it == models_.end()) {
        return nullptr; // 模型不存在时返回空指针
    }
    return it->second.get();
}


void ModelManager::readSpline(QUrl spline_path)
{
    auto mesh = ModelUtil::mesh_from_spline(spline_path.toLocalFile().toStdU16String());
    if (!mesh || mesh->numFaces() == 0) {
        //emit splineLoadFailed(tr("fail to load spline file."));
        qDebug() << "导入文件错误: " << spline_path;
    }

    // 重新分配 std::unique_ptr<Model>，并更新模型数据
    addModel(spline_path.fileName(), std::make_unique<Model>(std::move(mesh)));
}

void ModelManager::readMesh(QUrl target_mesh)
{
    auto mesh = ModelUtil::read_obj_with_groups(target_mesh.toLocalFile().toStdU16String());

    if (!mesh || mesh->numFaces() == 0) {
        //emit splineLoadFailed(tr("fail to load spline file."));
        qDebug() << "导入文件错误: " << target_mesh;
    }
    // 重新分配 std::unique_ptr<Model>，并更新模型数据
    addModel(target_mesh.fileName(), std::make_unique<Model>(std::move(mesh)));
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

Q_INVOKABLE void ModelManager::reName(const QString& oldName, const QString& newName){
        // 使用 find() 检查旧名称是否存在
        auto it_old = models_.find(oldName);
        if (it_old == models_.end()) {
            qDebug() << "模型不存在：" << oldName;
            return;
        }
        // 使用 find() 检查新名称是否已经被占用
        if (models_.find(newName) != models_.end()) {
            qDebug() << "新名称已存在：" << newName;
            return;
        }

        // 转移模型对象，并更新映射
        auto modelPtr = std::move(it_old->second);
        models_.erase(it_old);
        models_[newName] = std::move(modelPtr);

        // 发射信号通知名称已更新
        emit modelNameChanged(oldName, newName);
    }


//void ModelManager::connectVtk(const QString& modelName)
//{
//    auto model = getModel(modelName);
//    if (!model || !vtk_item_) {
//        qDebug() << "模型或 VTK 项不存在: " << modelName;
//        return;
//    }
//
//    // 传model指针或引用进lambda表达式、model内添加model_name成员，由model_name成员获取该模型的名字
//    connect(model, &Model::patchUpdated, [this,modelName](int patch_id, const std::vector<std::array<double, 3>>& points, const std::vector<std::array<int, 3>>& triangles) {
//        vtk_item_->patchUpdated(modelName, patch_id, points, triangles); });
//    connect(model, &Model::blockUpdated, [this, modelName](int block_id, const std::unordered_set<int>& block_patches) {
//        vtk_item_->blockUpdated(modelName, block_id, block_patches); });
//    connect(model, &Model::blocksMerged, [this, modelName](const std::vector<int>& block_ids, int father_block, const std::unordered_set<int>& father_block_patches) {
//        vtk_item_->blocksMerged(modelName, block_ids, father_block, father_block_patches); });
//    connect(model, &Model::groupUpdated, [this, modelName](int group_id, const std::unordered_set<int>& group_blocks) {
//        vtk_item_->groupUpdated(modelName, group_id, group_blocks); } );
//    connect(model, &Model::groupMerged, [this, modelName](const std::vector<int>& group_ids, int father_group, const std::unordered_set<int>& father_group_blocks) {
//        vtk_item_->groupMerged(modelName, group_ids, father_group, father_group_blocks); });
//    connect(model, &Model::modelInited, [this, modelName](const std::unordered_map<int, std::unique_ptr<Patch>>* patches,
//        const std::unordered_map<int, std::unique_ptr<Block>>* blocks,
//        const std::unordered_map<int, std::unique_ptr<Group>>* groups) 
//        {vtk_item_->onModelInited(modelName, patches, blocks, groups); });
//
//}
//void ModelManager::connectVtk(const QString& modelName)
//{
//    auto model = getModel(modelName);
//    if (!model || !vtk_item_) {
//        qDebug() << "模型或 VTK 项不存在: " << modelName;
//        return;
//    }
//
//    // 传model指针或引用进lambda表达式、model内添加model_name成员，由model_name成员获取该模型的名字
//    connect(model, &Model::patchUpdated, [this,modelName](int patch_id, const std::vector<std::array<double, 3>>& points, const std::vector<std::array<int, 3>>& triangles) {
//        vtk_item_->patchUpdated(modelName, patch_id, points, triangles); });
//    connect(model, &Model::blockUpdated, [this, modelName](int block_id, const std::unordered_set<int>& block_patches) {
//        vtk_item_->blockUpdated(modelName, block_id, block_patches); });
//    connect(model, &Model::blocksMerged, [this, modelName](const std::vector<int>& block_ids, int father_block, const std::unordered_set<int>& father_block_patches) {
//        vtk_item_->blocksMerged(modelName, block_ids, father_block, father_block_patches); });
//    connect(model, &Model::groupUpdated, [this, modelName](int group_id, const std::unordered_set<int>& group_blocks) {
//        vtk_item_->groupUpdated(modelName, group_id, group_blocks); } );
//    connect(model, &Model::groupMerged, [this, modelName](const std::vector<int>& group_ids, int father_group, const std::unordered_set<int>& father_group_blocks) {
//        vtk_item_->groupMerged(modelName, group_ids, father_group, father_group_blocks); });
//    connect(model, &Model::modelInited, [this, modelName](const std::unordered_map<int, std::unique_ptr<Patch>>* patches,
//        const std::unordered_map<int, std::unique_ptr<Block>>* blocks,
//        const std::unordered_map<int, std::unique_ptr<Group>>* groups) 
//        {vtk_item_->onModelInited(modelName, patches, blocks, groups); });
//
//}

