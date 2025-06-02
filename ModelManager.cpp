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
Index ModelManager::addModel(std::unique_ptr<ModelData> model)
{
    if (!model) {
        qDebug() << "添加空模型，跳过";
        return -1;
    }

    Index model_id = ++max_index_;
    model->setModelName(model->getModelName());
    model->id_ = model_id;
    models_[model_id] = std::move(model);

    if (observer_)
        observer_->notifyModelAdded(max_index_);
    return model_id;
}

// 删除模型
void ModelManager::removeModel(Index model_id) {
    auto it = models_.find(model_id);
    if (it == models_.end()) {
        throw std::runtime_error("ModelData with the given name does not exist.");
    }
    models_.erase(it);
    // 发射删除模型信号
    if (observer_)
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

std::optional<ModelOperator> ModelManager::getModelOperator(Index model_id) const
{
    ModelData* mesh = getModel(model_id);
    if (mesh) {
        return ModelOperator(mesh, observer_);
    }
    return {}; // 如果找不到模型，返回空指针
}