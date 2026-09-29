#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 自动几何间隙修复功能：检测并按全局清理容差 Stitch 跨面自由边。
 */
class AutoGeometryRepairHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;
};
}
