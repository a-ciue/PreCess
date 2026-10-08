#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 分割几何面功能：使用同一组件中的几何边或相交面切分目标面。
 */
class SplitFaceHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;
};
}
