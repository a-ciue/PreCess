#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 局部缝合功能：从种子自由边识别所属间隙边界，并缝合已有面。
 */
class FillGapHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;
};
}
