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
#include <QtQml/QQmlApplicationEngine>
#include <stdexcept>

// 添加模型
void ModelManager::addModel(const QString& model_name, std::unique_ptr<ModelData> model)
{
    if (!model) {
        qDebug() << "模型或 VTK 项不存在:" << model_name;
        return;
    }

    Index model_id = ++max_index_;
    model->setModelName(model_name);
    model->id_ = model_id;
    models_[model_id] = std::move(model);

    observer_->notifyModelAdded(max_index_);
}

// 删除模型
void ModelManager::removeModel(Index model_id) {
    auto it = models_.find(model_id);
    if (it == models_.end()) {
        throw std::runtime_error("ModelData with the given name does not exist.");
    }
    models_.erase(it);
    // 发射删除模型信号
    observer_->notifyModelRemoved(model_id);
}

// 获取模型
ModelData* ModelManager::getModel(Index model_id) const {
    auto it = models_.find(model_id);
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
        observer_->notifySplineLoadFailed(QStringLiteral("导入样条文件失败: ") + spline_path.toString());
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

void ModelManager::writeMesh(Index model_id, QUrl target_mesh, const QString& render_mode, const QString& extension)
{
    auto mesh = getModel(model_id);
    if (!mesh) {
        qDebug() << "未找到指定的模型: " << model_id;
        return;
    }
    FileHandler::instance().writeMesh(mesh, target_mesh.toLocalFile(), render_mode, extension);
}

void ModelManager::renameModel(Index model_id, const QString& new_name)
{
	if (!models_.count(model_id))
	{
        qDebug() << "模型不存在: " << model_id;
        return;
	}
        models_[model_id]->setModelName(new_name);

    // 发射信号通知名称已更新
    observer_->notifyModelNameChanged(model_id, new_name);
}

std::optional<ModelOperator> ModelManager::getModelOperator(Index model_id)
{
    ModelData* mesh = getModel(model_id);
    if (mesh) {
        return ModelOperator(mesh, observer_);
    }
    return {}; // 如果找不到模型，返回空指针
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

