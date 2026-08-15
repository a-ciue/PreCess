/**
 * @file FeatureEventGateway.h
 * @brief 功能事件网关：功能事件回调的操作边界
 */
#ifndef FEATURE_EVENT_GATEWAY_H
#define FEATURE_EVENT_GATEWAY_H
#include "EventBus.h"
#include "ModelLayer.h"
#include "UndoStack.h" // 模板方法内调用 beginOperation/commitOperation，需完整类型

#include <functional>
#include <string>
#include <utility>

namespace systems::feature {

/**
 * @brief 功能事件网关：包装 EventBus 订阅，回调返回后自动 flushNotifications（操作边界）
 *
 * 功能经 FeatureContext::events 订阅的事件回调由本网关包装：回调执行后统一
 * 经 ModelLayer::flushNotifications() 发出本次操作内的组件变更通知（写路径
 * 经 ComponentOperator 写必脏记入待通知集合；无写入则 flush 空转）。
 * 回调抛异常时先 flush 再重抛，保证部分写入的通知不丢。
 *
 * undo 模式绑定（构造时注入，每个 FeatureEntry 持有绑定本功能模式的视图）：
 * - Auto：回调包 beginOperation/commitOperation（边界自动记录）；异常路径 commit 后
 *   重抛——begin 过的操作在异常时提交而非丢弃：部分写入可撤销，与 flush 语义一致。
 * - Manual：只 flush，记录由插件经 ctx.undo 的 staged 会话自控。
 *
 * @note 订阅句柄与 core::EventBus::Subscription 同一类型，功能侧用法不变。
 */
class FeatureEventGateway {
public:
    /**
     * @param bus 事件总线
     * @param model 模型层（flushNotifications 边界）
     * @param undo_stack undo 栈（可空：无栈时退化为仅 flush）
     * @param undo_automatic 本功能的 undo 模式（Auto=边界自动记录；Manual=插件自控，只 flush）
     * @param undo_label 自动记录的显示名（功能显示名）
     */
    FeatureEventGateway(core::EventBus& bus, ModelLayer& model,
        UndoStack* undo_stack = nullptr, bool undo_automatic = true, std::string undo_label = {}) noexcept
        : bus_(&bus)
        , model_(&model)
        , undo_stack_(undo_stack)
        , undo_automatic_(undo_automatic)
        , undo_label_(std::move(undo_label))
    {
    }

    /**
     * @brief 订阅某类型事件（回调返回后自动 flushNotifications；Auto 模式包 undo 操作边界）
     * @tparam Event 事件类型，回调按 const& 接收
     * @param handler 事件处理函数，须保证其捕获的对象存活至退订
     * @return 订阅句柄；析构或 reset 时自动退订
     */
    template <typename Event>
    [[nodiscard]] core::EventBus::Subscription subscribe(std::function<void(const Event&)> handler)
    {
        ModelLayer& model = *model_;
        UndoStack* undo_stack = undo_stack_;
        const bool automatic = undo_automatic_;
        const std::string label = undo_label_;
        return bus_->subscribe<Event>(
            [handler = std::move(handler), &model, undo_stack, automatic, label](const Event& event) {
                if (undo_stack && automatic)
                    undo_stack->beginOperation(label);
                try {
                    handler(event);
                } catch (...) {
                    if (undo_stack && automatic)
                        undo_stack->commitOperation(); // 异常时提交而非丢弃：部分写入可撤销
                    model.flushNotifications(); // 异常路径同样 flush，部分写入通知不丢
                    throw;
                }
                if (undo_stack && automatic)
                    undo_stack->commitOperation();
                model.flushNotifications();
            });
    }

    //! @brief 仅需直接 publish 的场合逃生门
    core::EventBus& bus() noexcept { return *bus_; }

private:
    core::EventBus* bus_;
    ModelLayer* model_;
    UndoStack* undo_stack_;
    bool undo_automatic_;
    std::string undo_label_;
};
}
#endif // FEATURE_EVENT_GATEWAY_H
