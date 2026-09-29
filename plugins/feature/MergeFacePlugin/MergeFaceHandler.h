#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 合并几何面功能：将同一组件中连通且同域的多个面合并为一个面。
 */
class MergeFaceHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;
};
}
