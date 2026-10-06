/**
 * @file FeatureEventGateway.h
 * @brief 功能事件网关：预览写归属、订阅存活与通知 flush，不维护正式 undo 历史
 */
#ifndef FEATURE_EVENT_GATEWAY_H
#define FEATURE_EVENT_GATEWAY_H
#include "EventBus.h"
#include "FeatureEvents.h"
#include "ModelLayer.h"
#include "ModelScope.h"

#include <functional>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <type_traits>
#include <utility>

namespace systems::feature {

/**
 * @brief 包装事件订阅：回调只写自己的预览，结束或异常时 flush，不创建操作记录。
 * @note 参数事件只进入所属功能；全局观察经 bus() 显式订阅。
 * 占用期仅 ModelEvent 延后；用户命令不排队重放，延后执行前校验订阅存活。
 */
class FeatureEventGateway {
public:
    /** @brief 构造绑定功能身份与模型通知暂存器；无栈只负责通知，无暂存器同步投递。 */
    FeatureEventGateway(core::EventBus& bus, ModelLayer& model,
        UndoStack* undo_stack = nullptr, std::string owner = { },
        std::function<void(std::function<void()>)> defer = { }) noexcept
        : bus_(&bus)
        , model_(&model)
        , undo_stack_(undo_stack)
        , owner_(std::move(owner))
        , defer_(std::move(defer))
    {
    }

    /**
     * @brief 订阅事件；handler 捕获的对象须存活至退订。
     * @return 析构或 reset 时自动退订的句柄。
     */
    template <typename Event>
    [[nodiscard]] core::EventBus::Subscription subscribe(std::function<void(const Event&)> handler)
    {
        auto invoke = [handler = std::move(handler), model = model_, undo_stack = undo_stack_, owner = owner_](const Event& event) {
            model->assertOwnerThread();
            if constexpr (std::is_same_v<Event, ParameterChangedEvent>) {
                if (event.feature != owner)
                    return;
            }
            invokePreview(handler, event, *model, undo_stack, owner);
        };
        if constexpr (std::is_same_v<Event, ModelEvent>) {
            // 只有延后通知需要自持事件值与原订阅存活标记。
            auto lifetime = std::make_shared<int>(0);
            std::weak_ptr<int> alive = lifetime;
            return bus_->subscribe<Event>(
                [invoke = std::move(invoke), model = model_, defer = defer_,
                    lifetime = std::move(lifetime), alive](const Event& event) {
                    model->assertOwnerThread();
                    if (model->writesPending() && defer) {
                        defer([alive, invoke, event] {
                            if (!alive.expired())
                                invoke(event);
                        });
                    } else {
                        invoke(event);
                    }
                });
        } else {
            return bus_->subscribe<Event>(std::move(invoke));
        }
    }

    //! @brief 底层总线：全局参数观察显式从此订阅，回调不经功能预览网关。
    core::EventBus& bus() noexcept { return *bus_; }

private:
    template <typename Event>
    static void invokePreview(const std::function<void(const Event&)>& handler, const Event& event,
        ModelLayer& model, UndoStack* undo_stack, const std::string& owner)
    {
        ModelScope scope(model, undo_stack, { }, ModelScope::Kind::Preview, owner);
        try {
            handler(event);
        } catch (const ModelOperationBusy& e) {
            spdlog::warn("FeatureEventGateway: preview operation rejected: {}", e.what());
        }
    }
    core::EventBus* bus_;
    ModelLayer* model_;
    UndoStack* undo_stack_;
    std::string owner_;
    const std::function<void(std::function<void()>)> defer_;
};
}
#endif // FEATURE_EVENT_GATEWAY_H
