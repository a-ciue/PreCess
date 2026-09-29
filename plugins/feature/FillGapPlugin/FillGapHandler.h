#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 从种子自由边识别间隙，将选中侧重建到对侧边界。
 */
class FillGapHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;
};
}
