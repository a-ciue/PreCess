#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 自动几何间隙修复功能：显式选择一个组件，检测并按清理容差缝合该组件内的跨面自由边。
 */
class AutoGeometryRepairHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;
};
}
