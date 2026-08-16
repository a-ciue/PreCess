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
 * 生命周期：注册时 setup() 一次（声明参数/菜单/按键绑定 + 订阅事件、建立常驻状态）；
 * 随后功能随活动操作切换被反复 进入 activate() / 退出 deactivate()；
 * 注销时 teardown() 一次。
 * 菜单项触发 execute()；按键绑定命中时回调 onKeyEvent()。
 *
 * activate()/deactivate() 在 **GUI 线程** 调用，不得直接触碰交互状态与标注
 * （线程约定见 InteractionState.h）；交互现场清理经 InteractionContext::deferRefresh
 * 安排到渲染线程执行。框架定序保证：退出时 deactivate() 先于交互下线（setActive(false)）、
 * 进入时 activate() 先于交互上线（setActive(true)），deactivate() 中经 deferRefresh
 * 挂的清理在下线迁移时必被消费并触发视口重绘。
 * activate()/deactivate()/teardown() 中的模型写在回调返回后由框架统一 flush
 * （生命周期通知不成 undo 记录）。
 */
class FeatureHandler {
public:
    virtual ~FeatureHandler() = default;
    /**
     * @brief 注册时调用一次：声明参数、菜单项与按键绑定，并经 ctx.events 订阅事件、建立常驻状态
     * @note 实现内不得读取 ctx.params 的值：参数默认值在 setup() 返回后由系统载入
     */
    virtual void setup(FeatureRegistrar& reg, FeatureContext& ctx) { }
    /**
     * @brief 注销时调用一次：清理常驻状态（事件订阅句柄随 Subscription 析构自动退订时可为空实现）
     */
    virtual void teardown(FeatureContext& ctx) { }
    /**
     * @brief 功能进入（用户点选本功能为活动操作，GUI 线程）
     */
    virtual void activate(FeatureContext& ctx) { }
    /**
     * @brief 功能退出（活动操作切走或取消，GUI 线程）
     */
    virtual void deactivate(FeatureContext& ctx) { }
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
