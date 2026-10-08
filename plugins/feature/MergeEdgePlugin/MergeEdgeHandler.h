#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 合并几何边功能：将同一组件中连续且同域的多条边合并为一条边。
 */
class MergeEdgeHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;
};
}
