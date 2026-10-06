/**
 * @file ModelScope.cpp
 * @brief ModelScope 实现：收尾与退出原因无关，两动作各自兜异常
 */
#include "ModelScope.h"

#include <spdlog/spdlog.h>

ModelScope::ModelScope(ModelLayer& layer, UndoStack* stack, std::string label, Kind kind, std::string owner)
    : owner_scope_(stack, owner.empty() && stack ? stack->currentOwner() : std::move(owner))
    , layer_(&layer)
    , stack_((kind == Kind::Command || kind == Kind::Execute) ? stack : nullptr)
{
    layer.assertOwnerThread();
    if (kind == Kind::Cleanup)
        recording_pause_.emplace(stack);
    if (kind == Kind::Preview)
        preview_access_.emplace(stack);
    if (stack_)
        stack_->beginOperation(std::move(label), kind == Kind::Execute);
}

ModelScope::~ModelScope()
{
    // 收尾与退出原因无关（成功/异常同构）：两动作独立兜异常逐一执行——
    // 前一步异常不吞掉后续动作（边界必闭、通知必发；写授权与操作占用由宿主持有）
    if (stack_) {
        try {
            stack_->commitOperation();
        } catch (const std::exception& e) {
            spdlog::error("ModelScope: commitOperation failed: {}", e.what());
        } catch (...) {
            spdlog::error("ModelScope: commitOperation failed with unknown exception");
        }
    }
    recording_pause_.reset(); // 清理写段结束后，通知监听者不能继承无历史写权限。
    {
        try {
            layer_->flushNotifications();
        } catch (const std::exception& e) {
            spdlog::error("ModelScope: flushNotifications failed: {}", e.what());
        } catch (...) {
            spdlog::error("ModelScope: flushNotifications failed with unknown exception");
        }
    }
}
