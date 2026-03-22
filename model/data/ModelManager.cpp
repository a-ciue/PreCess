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
#include "ModelObserver.h"

#include <spdlog/spdlog.h>
#include <filesystem>
#include <stdexcept>

// 添加模型
Index ModelManager::addModel(std::unique_ptr<ModelData> model)
{
    if (!model) {
        spdlog::warn("Adding an empty model, skipping");
        return -1;
    }

    Index model_id = ++max_index_;

    // 补齐全局 component_id
    for (auto& c : model->components()) {
        if (!c)
            continue;
        if (c->id < 0) {
            c->id = allocateComponentId();
        }
    }
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


std::optional<ModelOperator> ModelManager::getModelOperator(Index model_id) const
{
    ModelData* mesh = getModel(model_id);
    if (mesh) {
        return ModelOperator(model_id, *mesh, observer_);
    }
    return {}; // 如果找不到模型，返回空指针
}

Index ModelManager::allocateComponentId() noexcept
{
    return next_component_id_++;
}

Component* ModelManager::findComponent(Index component_id) const
{
    for (const auto& [mid, model] : models_) {
        if (!model)
            continue;

        for (const auto& c : model->components()) {
            if (c && c->id == component_id) {
                return c.get();
            }
        }
    }
    return nullptr;
}