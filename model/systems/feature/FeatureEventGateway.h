/**
 * @file FeatureEventGateway.h
 * @brief 功能事件网关：功能事件回调的操作边界
 */
#ifndef FEATURE_EVENT_GATEWAY_H
#define FEATURE_EVENT_GATEWAY_H
#include "EventBus.h"
#include "ModelLayer.h"

#include <functional>
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
 * @note 订阅句柄与 core::EventBus::Subscription 同一类型，功能侧用法不变。
 */
class FeatureEventGateway {
public:
    FeatureEventGateway(core::EventBus& bus, ModelLayer& model) noexcept
        : bus_(&bus)
        , model_(&model)
    {
    }

    /**
     * @brief 订阅某类型事件（回调返回后自动 flushNotifications）
     * @tparam Event 事件类型，回调按 const& 接收
     * @param handler 事件处理函数，须保证其捕获的对象存活至退订
     * @return 订阅句柄；析构或 reset 时自动退订
     */
    template <typename Event>
    [[nodiscard]] core::EventBus::Subscription subscribe(std::function<void(const Event&)> handler)
    {
        ModelLayer& model = *model_;
        return bus_->subscribe<Event>(
            [handler = std::move(handler), &model](const Event& event) {
                try {
                    handler(event);
                } catch (...) {
                    model.flushNotifications(); // 异常路径同样 flush，部分写入通知不丢
                    throw;
                }
                model.flushNotifications();
            });
    }

    //! @brief 仅需直接 publish 的场合逃生门
    core::EventBus& bus() noexcept { return *bus_; }

private:
    core::EventBus* bus_;
    ModelLayer* model_;
};
}
#endif // FEATURE_EVENT_GATEWAY_H
