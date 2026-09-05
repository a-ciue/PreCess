#pragma once
#include "EventBus.h"
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 缩放预览演示功能：演示 undo staged 会话（Manual 模式）的预览/确认/取消流程
 *
 * json 声明 "undo": "manual"，invoke 边界只 flush 不自动记录；功能经 ctx.undo 的
 * staged 会话自控记录："预览"按钮开启会话（栈捕获 before₀）、按当前因子应用预览并
 * 开始监听参数变化，预览写不成 undo 记录；改因子经 revertStaged 回滚 before₀ 再按
 * 新绝对因子重写；菜单触发 execute() = 确认（commitStaged 记一条记录），"取消"按钮
 * cancelStaged 回滚不成记录。
 */
class ScalePreviewHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    void teardown(FeatureContext& ctx) override;
    void deactivate(FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;

private:
    // 从参数集同步"缩放因子"当前值
    void syncParams(FeatureContext& ctx);
    // 开 staged 会话并按当前因子预览（已有会话则回滚 before₀ 重预览；组件/网格缺失时不开会话）
    void startPreview(FeatureContext& ctx);
    // 按绝对因子就地缩放活动组件网格顶点坐标（预览写；调用前确认组件与网格存在）
    bool applyPreview(FeatureContext& ctx);

    FeatureContext* ctx_ { nullptr }; //> 功能上下文（生命周期由 FeatureSystem 管理，先于此 handler 销毁）
    core::EventBus::Subscription param_sub_; //> 参数变更事件订阅
    double scale_ { 1.0 }; //> "缩放因子"参数的当前值
};
}
