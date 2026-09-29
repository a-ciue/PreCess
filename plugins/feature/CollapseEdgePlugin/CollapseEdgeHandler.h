#pragma once
#include "FeatureHandler.h"
namespace systems::feature {
/** @brief 压缩几何边的功能处理器。 */
class CollapseEdgeHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;
};
}
