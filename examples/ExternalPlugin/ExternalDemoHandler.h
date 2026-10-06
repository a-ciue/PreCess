#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 外部插件示例功能：演示经 find_package(PreCess) 独立构建的功能插件最小实现
 *
 * 声明一个浮点参数与一个菜单项；执行时读取参数值并打印日志。
 * 事件、任务与预览写法参照 SDK examples 下的 FeatureDemoPlugin、TaskDemoPlugin、
 * ScalePreviewPlugin；视口交互参照配套源码中的 MeasurePlugin。
 */
class ExternalDemoHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;
};
}
