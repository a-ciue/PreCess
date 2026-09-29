#pragma once
#include "FeatureHandler.h"
namespace systems::feature {
/** @brief 按比例分割几何边的功能处理器。 */
class SplitEdgeHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;
};
}
