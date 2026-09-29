#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 几何缝合功能：合并容差内的两个点，或缝合两组自由边界链。
 */
class StitchGeometryHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;
};
}
