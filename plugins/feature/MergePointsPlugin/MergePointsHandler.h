#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 点合并功能：将容差内的两个几何点合并到第一点、第二点或中点。
 */
class MergePointsHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;
};
}
