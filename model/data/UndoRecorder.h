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

    /**
     * @brief 组件写前标脏回调（数据尚未修改，操作边界内首次标脏捕获 before-image）
     */
    virtual void onComponentDirty(Index component_id, const ComponentData& data) = 0;
    /**
     * @brief 组件与结构写共同的归属判定；事件只允许访问本 owner 预览
     *
     * 有归属 = 操作边界内、undo/redo 恢复中、或层（预览会话）自写——三种路径各自
     * 有记账/取消机制。返回 false 时模型层在写前直接拒绝：无归属写既不产生可撤销
     * 记录也不触发预览取消，属必须封死的违规路径（源码全量排查与告警日志均证明
     * 生产代码树零流量，违规只可能来自新增代码踩线）。
     * 默认放行：无归属判定能力的记录器实现不受本判定约束。
     */
    virtual bool allowsUnrecordedWrite() const { return true; }
    //! @brief addModel 完成后回调
    virtual void onModelAdded(Index model_id) = 0;
    //! @brief removeModel 执行前回调（可捕获整模型快照）
    virtual void onModelRemoving(const ModelData& model) = 0;
    //! @brief 组件加入模型后回调（ModelOperator::addGeometryComponent）
    virtual void onComponentAdded(Index model_id, Index component_id) = 0;
    //! @brief removeComponent 执行前回调（可克隆组件快照）
    virtual void onComponentRemoving(const ComponentData& component) = 0;
};
