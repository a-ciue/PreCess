/**
 * @file UndoStack.h
 * @brief 撤销/重做栈：混合记录模式（默认边界自动 + Manual 插件自控）
 *
 * 混合记录模式：
 * - 默认边界自动记录：操作边界（beginOperation/commitOperation）内组件首次标脏经
 *   ModelLayer 写前钩子捕获 before-image，commit 补 after-image 成一条记录；结构操作
 *   边界内并入当前操作、边界外即时自成记录。简单操作零插件代码。
 * - Manual 插件自控：feature 元数据声明 "undo": "manual" 后边界不再自动捕获/提交，
 *   插件经 staged 会话显式控制——分阶段操作是栈的一等概念，before-image 由栈持有：
 *   beginStaged（栈捕获 before₀）→ apply/preview（可反复，重试经 revertStaged 回滚）→
 *   commitStaged（before₀+当前状态记一条）/ cancelStaged（回滚不记录）；
 *   undo 与预览恢复共享同一份 before-image。
 *
 * staged 会话规则（v1 单组件，无快照链——逐步回退需求由 Auto 模式"每次执行一条记录"覆盖）：
 * - staged 打开时 undo() = cancelStaged()（恢复 before₀ 并关闭，消费本次，全局栈不动）；
 *   redo() 空转。
 * - staged 打开时的隐式 cancelStaged() 兜底挂在真实写入点（onComponentDirty 首次标脏、
 *   四个结构钩子），而非 beginOperation：纯旁观回调（只读事件订阅同样走
 *   操作边界）不误杀进行中的预览；旧功能后续 staged 调用发现会话已关 → 空转容忍
 *   （beginStaged 除外：会话冲突抛 std::runtime_error）。
 * - 导出为只读所见即所得（含预览态）；stagedActive() 暴露 QML 供界面禁用入口。
 * - staged 未关功能被停用属插件职责（deactivate 时关闭），写入点隐式 cancel 兜底。
 */
#pragma once
#include "Core.h"
#include "UndoRecord.h"
#include "UndoRecorder.h"

#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class ModelLayer;

class UndoStack : public UndoRecorder {
public:
    //! @brief 栈深度上限（before+after 双快照，大网格内存受控），溢出丢最旧
    static constexpr std::size_t kMaxDepth = 32;

    explicit UndoStack(ModelLayer& model);

    // —— 操作边界（自动模式）——
    //! @brief 嵌套深度+1（边界可嵌套，最外层生效）；不做 staged 隐式 cancel（推迟到真实写入点）
    void beginOperation(std::string label);
    //! @brief 深度-1；归零时：空操作丢弃，否则补 after-image、入栈、清空 redo
    void commitOperation();

    // —— staged 会话（Manual 模式，栈持有 before-image）——
    //! @brief 栈捕获 before₀；组件不存在返回 false；已有 staged 抛 std::runtime_error
    bool beginStaged(std::string label, Index component_id);
    //! @brief 确认：before₀+当前状态成一条记录入栈；无 staged 空转
    void commitStaged();
    //! @brief 恢复 before₀ 并关闭会话，不成记录；无 staged 空转
    void cancelStaged();
    //! @brief 恢复 before₀，会话保持打开（纯预览重试）；无 staged 空转
    void revertStaged();
    //! @brief 是否有进行中的 staged 会话（暴露 QML 供界面禁用导出/切换）
    bool stagedActive() const;

    // —— 撤销/重做（自身即边界：恢复后 flushNotifications）——
    bool canUndo() const;
    bool canRedo() const;
    std::optional<std::string> undoLabel() const;
    std::optional<std::string> redoLabel() const;
    //! @brief staged 打开 = cancelStaged（恢复 before₀ 并关闭，消费本次，全局栈不动）；否则正常撤销
    void undo();
    //! @brief staged 打开空转；否则正常重做
    void redo();
    void clear();

    // —— UndoRecorder 钩子（ModelLayer 回调）——
    void onComponentDirty(Index component_id, const ComponentData& data) override; //!< 操作内首次标脏捕获 before；staged 打开先隐式 cancel
    void onModelAdded(Index model_id) override;
    void onModelRemoving(const ModelData& model) override; //!< removeModel 前捕获 ModelSnapshot
    void onComponentAdded(Index model_id, Index component_id) override;
    void onComponentRemoving(const ComponentData& component) override; //!< removeComponent 前克隆

    //! @brief 设置栈内容变更回调（入栈/撤销/重做/清空/staged 状态变化时触发；app 层 QML 刷新用）
    void setOnChanged(std::function<void()> callback);

private:
    //! @brief staged 会话状态（全局唯一，规则见文件头注释）
    struct StagedSession {
        std::string label;
        Index component_id { -1 };
        std::unique_ptr<ComponentData> before; //!< before₀：会话起点，commit/cancel/revert 时消费
    };

    //! @brief 入栈（超 kMaxDepth 丢最旧）、清空 redo、触发变更回调
    void pushRecord(UndoRecord record);
    //! @brief 结构钩子归属：边界内并入当前操作（一次用户动作一条记录）；边界外即时自成记录
    void recordStructural(StructuralEntry entry);
    //! @brief staged 打开则隐式 cancelStaged（真实写入点兜底，见文件头注释）
    void cancelStagedIfActive();
    //! @brief 组件存在则恢复快照（gid 对账内建于 restoreSnapshot），返回是否恢复
    bool restoreComponentSnapshot(Index component_id, const ComponentData& snapshot);

    ModelLayer* model_;
    bool applying_ { false }; //!< undo/redo/cancelStaged 应用中：抑制一切记录（恢复会回触钩子）
    int op_depth_ { 0 };
    std::string op_label_;
    std::vector<ComponentEntry> op_components_;
    std::vector<StructuralEntry> op_structural_; //!< 边界内结构操作并入当前操作
    std::optional<StagedSession> staged_; //!< 全局唯一 staged 会话
    std::deque<UndoRecord> undo_, redo_;
    std::function<void()> on_changed_;
};
