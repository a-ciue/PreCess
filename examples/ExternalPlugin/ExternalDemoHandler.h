#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 外部插件示例功能：演示经 find_package(PreCess) 独立构建的功能插件最小实现
 *
 * 声明一个浮点参数与一个菜单项；执行时读取参数值并打印日志。
 * 更完整的功能写法（事件订阅、交互、staged undo 等）参照 PreCess 源码树的
 * plugins/feature/FeatureDemoPlugin、MeasurePlugin、ScalePreviewPlugin。
 */
class ExternalDemoHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;
};
}
