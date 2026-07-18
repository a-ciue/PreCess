/**
 * @file FeatureHandler.h
 * @brief 功能系统的功能接口
 */
#ifndef FEATURE_HANDLER_H
#define FEATURE_HANDLER_H
#include <any>

namespace systems::feature {
class FeatureRegistrar;
struct FeatureContext;
struct KeyEvent;

/**
 * @brief 功能系统的功能接口，插件继承它实现具体功能
 *
 * 生命周期：注册时先 setup() 声明参数/菜单/按键绑定，再 activate() 订阅事件、
 * 建立功能状态；注销时 deactivate() 清理。
 * 菜单项触发 execute()；按键绑定命中时回调 onKeyEvent()。
 * 功能也可在 activate() 中直接订阅 FeatureContext::events 上的原始事件
 * （如 KeyEvent 的按下/释放流、ParameterChangedEvent、ModelEvent）。
 */
class FeatureHandler {
public:
    virtual ~FeatureHandler() = default;
    /**
     * @brief 声明功能的参数、菜单项与按键绑定（注册时调用一次）
     */
    virtual void setup(FeatureRegistrar& reg) { }
    /**
     * @brief 功能激活，在此通过 ctx.events 订阅事件
     */
    virtual void activate(FeatureContext& ctx) { }
    /**
     * @brief 功能停用，清理状态（事件订阅句柄随 Subscription 析构自动退订时可为空实现）
     */
    virtual void deactivate() { }
    /**
     * @brief 菜单触发的功能执行
     * @return 功能结果（可自定义类型）
     */
    virtual std::any execute(FeatureContext& ctx) { return {}; }
    /**
     * @brief 按键绑定命中后的回调（仅按下时触发）
     * @return true 表示功能已消费该事件，事件不再向焦点控件等其他对象传递；
     *         false 表示未处理，事件继续正常传递
     */
    virtual bool onKeyEvent(const KeyEvent& event) { return false; }
};
}
#endif // FEATURE_HANDLER_H
