/**
 * @file EventBus.h
 * @brief 类型安全的轻量级事件总线（发布/订阅）
 */
#ifndef EVENT_BUS_H
#define EVENT_BUS_H
#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace core {
/**
 * @brief 类型安全的轻量级事件总线，以事件类型为键分发事件
 *
 * 订阅返回 RAII 句柄，句柄析构（或 reset）时自动退订，避免悬挂回调。
 * 句柄可比总线活得久（如 UI 桥接订阅先于持有总线的会话析构）：总线析构后
 * 句柄退订自动失效，不再访问已析构的总线。
 * 采用组合而非单例：由上层持有实例并以引用注入各使用方。
 *
 * @note 非线程安全，约定仅在 GUI 线程发布与订阅（与 ModelObserver 的通知模型一致）。
 */
class EventBus {
public:
    using SubscriptionId = std::uint64_t;
    struct LifetimeToken; //> 存活标记（定义见私有区），订阅句柄持其弱引用探测总线存活

    /**
     * @brief 订阅句柄，析构时自动退订；仅可移动
     */
    class Subscription {
    public:
        Subscription() = default;
        ~Subscription() { reset(); }
        Subscription(const Subscription&) = delete;
        Subscription& operator=(const Subscription&) = delete;
        Subscription(Subscription&& other) noexcept
            : bus_(std::exchange(other.bus_, nullptr))
            , lifetime_(std::move(other.lifetime_))
            , type_(other.type_)
            , id_(std::exchange(other.id_, 0))
        {
        }
        Subscription& operator=(Subscription&& other) noexcept
        {
            if (this != &other) {
                reset();
                bus_ = std::exchange(other.bus_, nullptr);
                lifetime_ = std::move(other.lifetime_);
                type_ = other.type_;
                id_ = std::exchange(other.id_, 0);
            }
            return *this;
        }
        /**
         * @brief 主动退订，退订后句柄失效；总线已析构时句柄静默失效
         */
        void reset()
        {
            // 经生命周期令牌探测总线是否存活：总线先于句柄析构时跳过退订
            if (bus_) {
                if (auto lifetime = lifetime_.lock())
                    bus_->unsubscribe(type_, id_);
                bus_ = nullptr;
            }
        }
        /**
         * @brief 订阅是否仍然有效
         */
        explicit operator bool() const noexcept { return bus_ != nullptr; }

    private:
        friend class EventBus;
        Subscription(EventBus& bus, std::type_index type, SubscriptionId id) noexcept
            : bus_(&bus)
            , lifetime_(bus.lifetime_)
            , type_(type)
            , id_(id)
        {
        }
        EventBus* bus_ { nullptr };
        std::weak_ptr<const LifetimeToken> lifetime_; //> 总线生命周期令牌（探测总线存活）
        std::type_index type_ { typeid(void) };
        SubscriptionId id_ { 0 };
    };

    /**
     * @brief 订阅某类型事件
     * @tparam Event 事件类型，回调按 const& 接收
     * @param handler 事件处理函数，须保证其捕获的对象存活至退订
     * @return 订阅句柄；析构或 reset 时自动退订
     */
    template <typename Event>
    [[nodiscard]] Subscription subscribe(std::function<void(const Event&)> handler)
    {
        const auto type = std::type_index(typeid(Event));
        const SubscriptionId id = next_id_++;
        subscribers_[type].emplace_back(id, [handler = std::move(handler)](const void* event) {
            handler(*static_cast<const Event*>(event));
        });
        return Subscription(*this, type, id);
    }

    /**
     * @brief 同步发布事件给当前所有订阅者
     * @tparam Event 事件类型
     * @note 先快照订阅者列表再逐个调用，回调中可安全地订阅或退订；
     *       快照意味着本次发布中退订的订阅者仍会收到本次事件
     */
    template <typename Event>
    void publish(const Event& event)
    {
        auto it = subscribers_.find(std::type_index(typeid(Event)));
        if (it == subscribers_.end()) {
            return;
        }
        const auto handlers = it->second; //> 快照，允许回调内退订
        for (const auto& entry : handlers) {
            entry.second(&event);
        }
    }

private:
    void unsubscribe(std::type_index type, SubscriptionId id)
    {
        auto it = subscribers_.find(type);
        if (it == subscribers_.end()) {
            return;
        }
        auto& handlers = it->second;
        handlers.erase(
            std::remove_if(handlers.begin(), handlers.end(),
                [id](const auto& entry) { return entry.first == id; }),
            handlers.end());
        if (handlers.empty()) {
            subscribers_.erase(it);
        }
    }

    using ErasedHandler = std::function<void(const void*)>;
    struct LifetimeToken { }; //> 仅作存活标记
    std::unordered_map<std::type_index, std::vector<std::pair<SubscriptionId, ErasedHandler>>> subscribers_;
    SubscriptionId next_id_ { 1 };
    //> 生命周期令牌：订阅句柄持其弱引用探测总线存活。声明在最后（最先析构），
    //> 使总线的析构过程尽早对所有句柄表现为已失效
    std::shared_ptr<LifetimeToken> lifetime_ { std::make_shared<LifetimeToken>() };
};
}
#endif // EVENT_BUS_H
