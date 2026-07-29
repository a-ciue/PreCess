/**
 * @file InteractionState.h
 * @brief 视口交互状态：功能经上下文订阅回调、产出标注；渲染层据此驱动与拉取
 */
#ifndef INTERACTION_STATE_H
#define INTERACTION_STATE_H

#include "InteractiveTypes.h"

#include <atomic>
#include <functional>

namespace systems::interaction {

/**
 * @brief 单个功能的视口交互状态：回调订阅（功能写入）+ 标注与结果（渲染层拉取）
 *
 * 线程约定（与 EventBus 不同，修改前必读）：
 * - 全部回调由 **渲染线程**调用（InteractionService 经 dispatch_async 驱动）；
 * - 功能在 **GUI 线程** 的 activate() 中经 InteractionContext 注册回调与初始状态；
 * - GUI 线程的交互状态/标注变更经 setActive/requestRefresh 置位 + notify，
 *   渲染线程 syncPending 承接激活迁移与标注拉取（跨线程标志为 atomic）；
 * - 注册先行、调用在后，会话启停与选择系统的互斥由 UI 层保证（同原 InteractiveHandler 约定）。
 */
struct InteractionState {
    std::atomic<bool> active { false }; //> 交互激活态：功能经 setActive 写入（GUI 线程），渲染层读取路由拾取
    std::atomic<bool> needs_refresh { false }; //> GUI 线程变更交互状态/标注后置位，渲染层拉取后复位
    std::function<void()> on_activate; //> 交互会话开始（通常清空功能内部状态）
    std::function<void()> on_deactivate; //> 交互会话结束
    //! @brief 左键拾取：返回是否有状态变化（需要刷新标注）
    std::function<bool(const PickInfo&)> on_pick;
    //! @brief 悬停：返回是否更新了预览（需要刷新标注）
    std::function<bool(const PickInfo&)> on_hover;

    AnnotationBatch annotations; //> 功能在回调中直接更新，渲染层事件后拉取绘制
    //! @brief GUI 线程延迟操作：渲染线程 syncPending 消费执行后拉取标注刷新
    //!（覆盖语义：重复置位仅保留最新，操作须幂等）
    std::function<void()> deferred_op;

    //! @brief 会话结束时的渲染层清理：清空标注（不动订阅，订阅随功能常驻）
    void clearSession()
    {
        annotations.clear();
    }
};

} // namespace systems::interaction

#endif // INTERACTION_STATE_H
