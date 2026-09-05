/**
 * @file UndoRecorder.h
 * @brief 模型层 undo 记录钩子接口
 *
 * ModelLayer 在写路径与结构操作时机回调本接口，由 UndoStack 实现完成记录。
 * 未挂接记录器时（默认 nullptr）各回调点零开销。
 */
#pragma once
#include "Core.h"

struct ComponentData;
class ModelData;

/**
 * @brief undo 记录钩子（ModelLayer 回调，UndoStack 实现）
 *
 * 回调时机约定：写路径在数据修改前回调（可捕获 before-image）；
 * removeXxx 在执行前回调（可捕获快照），addXxx 在执行后回调。
 */
class UndoRecorder {
public:
    virtual ~UndoRecorder() = default;

    //! @brief 组件写前标脏回调（数据尚未修改，操作边界内首次标脏捕获 before-image）
    virtual void onComponentDirty(Index component_id, const ComponentData& data) = 0;
    //! @brief addModel 完成后回调
    virtual void onModelAdded(Index model_id) = 0;
    //! @brief removeModel 执行前回调（可捕获整模型快照）
    virtual void onModelRemoving(const ModelData& model) = 0;
    //! @brief 组件加入模型后回调（ModelOperator::addGeometryComponent）
    virtual void onComponentAdded(Index model_id, Index component_id) = 0;
    //! @brief removeComponent 执行前回调（可克隆组件快照）
    virtual void onComponentRemoving(const ComponentData& component) = 0;
};
