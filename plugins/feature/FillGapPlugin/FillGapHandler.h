#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 从种子自由边识别边界，按选项缝合已有面或创建平面/曲面补面。
 */
class FillGapHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;
};
}
