/**
 * @file InteractionContext.h
 * @brief 视口交互上下文：功能订阅拾取/悬停回调、产出标注的入口
 */
#ifndef INTERACTION_CONTEXT_H
#define INTERACTION_CONTEXT_H

#include "InteractionState.h"

#include <functional>

namespace systems::feature {

/**
 * @brief 视口交互上下文（FeatureContext 成员）：功能在 activate() 中经它订阅交互回调
 *
 * 与 ctx.events（EventBus，仅 GUI 线程）不同：交互回调由 **渲染线程** 直调，
 * 标注由功能在回调中直接更新，渲染层事件后拉取绘制（见 InteractionState 注释）。
 * 功能只需订阅关心的事件，无需继承任何接口。
 */
class InteractionContext {
public:
    explicit InteractionContext(systems::interaction::InteractionState& state);

    //! @brief 订阅交互会话开始（渲染线程；通常在此清空功能内部状态）
    void onActivate(std::function<void()> cb);
    //! @brief 订阅交互会话结束（渲染线程）
    void onDeactivate(std::function<void()> cb);
    //! @brief 订阅"清除"（面板清除按钮，渲染线程）
    void onClear(std::function<void()> cb);
    //! @brief 订阅左键拾取（渲染线程；返回是否有状态变化需要刷新标注）
    void onPick(std::function<bool(const systems::interaction::PickInfo&)> cb);
    //! @brief 订阅悬停（渲染线程；返回是否更新预览需要刷新标注）
    void onHover(std::function<bool(const systems::interaction::PickInfo&)> cb);

    //! @brief 标注集：功能在回调中直接更新，渲染层拉取绘制
    systems::interaction::AnnotationBatch& annotations();

    //! @brief 设置本功能交互激活态（单激活：激活自己时会先下线其他功能的交互）
    void setActive(bool on);

    //! @brief 由 FeatureSystem 装配时注入：激活本功能前下线其他功能的交互（单激活约定）
    std::function<void()> deactivate_others_;

private:
    systems::interaction::InteractionState* state_;
};

} // namespace systems::feature

#endif // INTERACTION_CONTEXT_H
