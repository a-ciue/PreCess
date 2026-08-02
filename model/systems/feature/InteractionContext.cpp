/**
 * @file InteractionContext.cpp
 * @brief 视口交互上下文实现
 */
#include "InteractionContext.h"

namespace systems::feature {

InteractionContext::InteractionContext(systems::interaction::InteractionState& state)
    : state_(&state)
{
}

void InteractionContext::onActivate(std::function<void()> cb)
{
    state_->on_activate = std::move(cb);
}

void InteractionContext::onDeactivate(std::function<void()> cb)
{
    state_->on_deactivate = std::move(cb);
}

void InteractionContext::onPick(std::function<bool(const systems::interaction::PickInfo&)> cb)
{
    state_->on_pick = std::move(cb);
}

void InteractionContext::onHover(std::function<bool(const systems::interaction::PickInfo&)> cb)
{
    state_->on_hover = std::move(cb);
}

systems::interaction::AnnotationBatch& InteractionContext::annotations()
{
    return state_->annotations;
}

void InteractionContext::requestRefresh()
{
    // 合并语义：needs_refresh 仍为 true 说明渲染侧尚未消费、必有在途 syncPending，
    // 跳过重复 notify，由在途刷新一并拉取（负载先于置位写入，消费者可见最新值）
    if (state_->needs_refresh.exchange(true))
        return;
    if (render_refresh_)
        render_refresh_();
}

void InteractionContext::deferRefresh(std::function<void()> pre_op)
{
    // 负载先于置位写入：渲染线程消费 deferred_op 时可见最新操作
    state_->deferred_op = std::move(pre_op);
    requestRefresh();
}

void InteractionContext::setActive(bool on)
{
    // 幂等守卫：目标状态已达则直接返回，重复启停无副作用
    if (state_->active == on)
        return;
    // 单激活约定：激活自己前先下线其他功能的交互
    if (on && deactivate_others_) {
        deactivate_others_();
    }
    state_->active = on;
    // 启停均通知渲染层：syncPending 经 syncState 承接迁移（上线 on_activate/吸附切换，下线 on_deactivate/清理）
    requestRefresh();
}

} // namespace systems::feature
