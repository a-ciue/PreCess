/**
 * @file UndoRecord.h
 * @brief 撤销/重做记录模型：一次操作 = 一条记录
 *
 * 记录由组件数据变更（双向快照）与结构变更（四选一）组成；
 * addModel → {ModelAdded}（undo=removeModel，undo 时捕获快照供 redo）；
 * removeModel → {ModelRemoved, takeModelSnapshot}；
 * addGeometryComponent → {ComponentAdded}（undo=removeComponent，undo 时克隆供 redo）；
 * removeComponent → {ComponentRemoved, clone}。
 */
#pragma once
#include "ComponentData.h" // unique_ptr<ComponentData> 值成员需要完整类型
#include "Core.h"
#include "ModelSnapshot.h" // unique_ptr<ModelSnapshot> 值成员需要完整类型

#include <memory>
#include <string>
#include <vector>

//! @brief 组件数据变更（双向快照）
struct ComponentEntry {
    Index component_id { -1 };
    std::unique_ptr<ComponentData> before; //!< first-dirty 时克隆（写前状态）
    std::unique_ptr<ComponentData> after; //!< commit 时克隆（写后状态）
};

//! @brief 结构变更（四选一）
struct StructuralEntry {
    enum class Kind { ModelAdded, ModelRemoved, ComponentAdded, ComponentRemoved };
    Kind kind;
    Index model_id { -1 };
    Index component_id { -1 };
    std::unique_ptr<ModelSnapshot> model_snapshot; //!< ModelRemoved 记录时捕获；ModelAdded undo 时捕获（供 redo）
    std::unique_ptr<ComponentData> component_snapshot; //!< ComponentRemoved 记录时捕获；ComponentAdded undo 时捕获（供 redo）
};

//! @brief 一次操作 = 一条记录
struct UndoRecord {
    std::string label;
    std::vector<ComponentEntry> components;
    std::vector<StructuralEntry> structural;

    bool empty() const { return components.empty() && structural.empty(); }
};
