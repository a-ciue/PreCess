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

void InteractionContext::onClear(std::function<void()> cb)
{
    state_->on_clear = std::move(cb);
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

void InteractionContext::setActive(bool on)
{
    // 单激活约定：激活自己前先下线其他功能的交互
    if (on && deactivate_others_) {
        deactivate_others_();
    }
    state_->active = on;
}

} // namespace systems::feature
