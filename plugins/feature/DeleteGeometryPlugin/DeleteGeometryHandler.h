#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 删除几何功能：删除一个顶层独立几何点、边、面或体，可选择是否同时删除其独占下级拓扑
 */
class DeleteGeometryHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;
};
}
