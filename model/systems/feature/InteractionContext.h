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
    //! @brief 订阅左键拾取（渲染线程；返回是否有状态变化需要刷新标注）
    void onPick(std::function<bool(const systems::interaction::PickInfo&)> cb);
    //! @brief 订阅悬停（渲染线程；返回是否更新预览需要刷新标注）
    void onHover(std::function<bool(const systems::interaction::PickInfo&)> cb);

    //! @brief 标注集：功能在回调中直接更新，渲染层拉取绘制
    systems::interaction::AnnotationBatch& annotations();

    //! @brief GUI 线程交互状态/标注变更后请求渲染刷新（渲染层 syncPending 拉取并复位 needs_refresh）
    //! @note 合并语义：needs_refresh 已置位（渲染侧尚未消费）时跳过重复 notify，由在途刷新一并拉取
    void requestRefresh();
    //! @brief GUI 线程延迟刷新：将刷新前置操作存入 InteractionState，经 requestRefresh 通知渲染线程执行后拉取标注
    //! @param pre_op 刷新前需在渲染线程执行的操作（如清理功能交互状态；覆盖语义，须幂等）
    void deferRefresh(std::function<void()> pre_op);

    //! @brief 设置本功能交互激活态（单激活：激活自己时会先下线其他功能的交互）
    void setActive(bool on);

private:
    // 装配注入仅 FeatureSystem 可用：功能只调用、不可覆盖系统接线（冻结后渲染线程调用无重赋值竞争）
    friend class FeatureSystem;

    //! @brief 由 FeatureSystem 装配时注入：激活本功能前下线其他功能的交互（单激活约定）
    std::function<void()> deactivate_others_;
    //! @brief 由 FeatureSystem 装配时注入：通知渲染层拉取标注并刷新视口
    std::function<void()> render_refresh_;

    systems::interaction::InteractionState* state_;
};

} // namespace systems::feature

#endif // INTERACTION_CONTEXT_H
