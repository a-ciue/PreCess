/**
 * @file ModelManager.cpp
 * @brief 实现 ModelManager 类，用于管理多个网格模型
 *
 * 该文件包含 ModelManager 类的实现，提供多模型管理功能，包括：
 * - 添加、删除和获取模型
 * - 维护与 VTK 组件的交互
 *
 * @author 徐昊阳 haoyangxu06@gmail.com
 * @date 2025/3/20
 */
#include "ModelManager.h"
#include "FileHandler.h"

#include <QObject>
#include <filesystem>

// 添加模型
void ModelManager::addModel(const QString& modelName, std::unique_ptr<ModelData> model)
{
    if (models_.find(modelName) != models_.end()) {
        throw std::runtime_error("ModelData with the given name already exists.");
    }

    // 为模型设置名称
    model->setModelName(modelName);
    models_[modelName] = std::move(model);

    ModelData* rawModel = models_[modelName].get();
    if (!rawModel) {
        qDebug() << "模型或 VTK 项不存在:" << modelName;
        return;
    }

    // 调用模型刷新接口，确保 VTK 数据更新
    rawModel->refreshVtk();

    // 发射添加模型的信号（需要在 ModelManager.h 中声明信号 modelAdded(const QString&)）
    emit modelAdded(modelName);
}

// 删除模型
void ModelManager::removeModel(const QString& modelName) {
    auto it = models_.find(modelName);
    if (it == models_.end()) {
        throw std::runtime_error("ModelData with the given name does not exist.");
    }
    models_.erase(it);
    // 发射删除模型信号
    emit modelRemoved(modelName);
}

// 获取模型
ModelData* ModelManager::getModel(const QString& modelName) const {
    auto it = models_.find(modelName);
    if (it == models_.end()) {
        return nullptr; // 模型不存在时返回空指针
    }
    return it->second.get();
}


void ModelManager::readSpline(QUrl spline_path)
{
    auto mesh = FileHandler::instance().readSpline(spline_path);
    if (!mesh) {
        qDebug() << "导入文件错误: " << spline_path;
        emit splineLoadFailed(QStringLiteral("导入样条文件失败: ") + spline_path.toString());
        return;
    }
    addModel(spline_path.fileName(), std::move(mesh));
}

void ModelManager::readMesh(QUrl target_mesh)
{
    auto mesh = FileHandler::instance().readMesh(target_mesh);
    if (!mesh) {
        qDebug() << "导入文件错误: " << target_mesh;
        return;
    }
    addModel(target_mesh.fileName(), std::move(mesh));
}

void ModelManager::writeMesh(const QString& modelName, QUrl target_mesh, QString renderMode, QString extension)
{
    auto mesh = getModel(modelName);
    if (!mesh) {
        qDebug() << "未找到指定的模型: " << modelName;
        return;
    }
    FileHandler::instance().writeMesh(mesh, target_mesh.toLocalFile(), renderMode, extension);
}

Q_INVOKABLE void ModelManager::renameModel(const QString& oldName, const QString& newName){
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

        auto modelPtr = std::move(it_old->second);
        // 更新Model类的model_name成员变量
        modelPtr->setModelName(newName);

        // 转移模型对象，并更新映射
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
//    connect(model, &ModelData::patchUpdated, [this,modelName](int patch_id, const std::vector<std::array<double, 3>>& points, const std::vector<std::array<int, 3>>& triangles) {
//        vtk_item_->patchUpdated(modelName, patch_id, points, triangles); });
//    connect(model, &ModelData::blockUpdated, [this, modelName](int block_id, const std::unordered_set<int>& block_patches) {
//        vtk_item_->blockUpdated(modelName, block_id, block_patches); });
//    connect(model, &ModelData::blocksMerged, [this, modelName](const std::vector<int>& block_ids, int father_block, const std::unordered_set<int>& father_block_patches) {
//        vtk_item_->blocksMerged(modelName, block_ids, father_block, father_block_patches); });
//    connect(model, &ModelData::groupUpdated, [this, modelName](int group_id, const std::unordered_set<int>& group_blocks) {
//        vtk_item_->groupUpdated(modelName, group_id, group_blocks); } );
//    connect(model, &ModelData::groupMerged, [this, modelName](const std::vector<int>& group_ids, int father_group, const std::unordered_set<int>& father_group_blocks) {
//        vtk_item_->groupMerged(modelName, group_ids, father_group, father_group_blocks); });
//    connect(model, &ModelData::modelInited, [this, modelName](const std::unordered_map<int, std::unique_ptr<Patch>>* patches,
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
//    connect(model, &ModelData::patchUpdated, [this,modelName](int patch_id, const std::vector<std::array<double, 3>>& points, const std::vector<std::array<int, 3>>& triangles) {
//        vtk_item_->patchUpdated(modelName, patch_id, points, triangles); });
//    connect(model, &ModelData::blockUpdated, [this, modelName](int block_id, const std::unordered_set<int>& block_patches) {
//        vtk_item_->blockUpdated(modelName, block_id, block_patches); });
//    connect(model, &ModelData::blocksMerged, [this, modelName](const std::vector<int>& block_ids, int father_block, const std::unordered_set<int>& father_block_patches) {
//        vtk_item_->blocksMerged(modelName, block_ids, father_block, father_block_patches); });
//    connect(model, &ModelData::groupUpdated, [this, modelName](int group_id, const std::unordered_set<int>& group_blocks) {
//        vtk_item_->groupUpdated(modelName, group_id, group_blocks); } );
//    connect(model, &ModelData::groupMerged, [this, modelName](const std::vector<int>& group_ids, int father_group, const std::unordered_set<int>& father_group_blocks) {
//        vtk_item_->groupMerged(modelName, group_ids, father_group, father_group_blocks); });
//    connect(model, &ModelData::modelInited, [this, modelName](const std::unordered_map<int, std::unique_ptr<Patch>>* patches,
//        const std::unordered_map<int, std::unique_ptr<Block>>* blocks,
//        const std::unordered_map<int, std::unique_ptr<Group>>* groups) 
//        {vtk_item_->onModelInited(modelName, patches, blocks, groups); });
//
//}

