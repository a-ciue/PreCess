/**
 * @file ModelScope.h
 * @brief 模型操作作用域（RAII）：模型写段的记账与通知收尾
 */
#pragma once
#include "ModelLayer.h"
#include "UndoStack.h"

#include <optional>
#include <string>

/**
 * @brief 模型操作作用域：undo 边界与通知 flush 的组合生命周期
 *
 * 固定入口：Command 记录正式操作，Execute 收尾吸收自己的预览，
 * Preview 只写本 owner 临时层，Notify 只 flush，Cleanup 明确抑制生命周期历史。
 * 同步嵌套共用正式捕获；所有入口退出均 flush，异常退出保留部分效果的可撤销语义。
 * 记账先于通知，两步分别兜异常；占用与写授权由宿主维护，通知不继承写授权。
 * 无栈时只 flush，不授予绕过模型写闸的权限。
 */
class ModelScope {
public:
    enum class Kind { Command,
        Execute,
        Preview,
        Notify,
        Cleanup };
    /** @brief 固定框架入口；Execute 吸收预览，Preview 只允许本 owner 预览写，Notify 只 flush。 */
    ModelScope(ModelLayer& layer, UndoStack* stack, std::string label,
        Kind kind = Kind::Command, std::string owner = { });
    ~ModelScope();

    ModelScope(const ModelScope&) = delete;
    ModelScope& operator=(const ModelScope&) = delete;

private:
    UndoStack::OwnerScope owner_scope_;
    std::optional<UndoStack::PreviewAccess> preview_access_;
    std::optional<UndoStack::RecordingPause> recording_pause_;
    ModelLayer* layer_;
    UndoStack* stack_; //!< 正式入口且非空（构造时裁决）
};
