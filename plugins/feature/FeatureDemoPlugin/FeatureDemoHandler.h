#pragma once
#include "EventBus.h"
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 功能系统演示功能：演示参数注册、菜单注册、按键绑定与事件实时响应
 */
class FeatureDemoHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    void teardown(FeatureContext& ctx) override;
    void activate(FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;
    bool onKeyEvent(const KeyEvent& event) override;

private:
    // 从参数集同步当前参数值
    void syncParams(FeatureContext& ctx);
    // 按"缩放因子"缩放当前活动组件的网格顶点坐标（演示修改模型对象）
    void applyScale(FeatureContext& ctx);

    FeatureContext* ctx_ { nullptr }; //> 功能上下文（生命周期由 FeatureSystem 管理，先于此 handler 销毁）
    core::EventBus::Subscription param_sub_; //> 参数变更事件订阅
    core::EventBus::Subscription model_sub_; //> 模型事件订阅
    double scale_ { 1.0 }; //> "缩放因子"参数的当前值
    bool auto_apply_ { false }; //> "自动应用"参数的当前值
};
}
