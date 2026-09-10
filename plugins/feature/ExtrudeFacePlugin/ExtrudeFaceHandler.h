#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 拉伸面为实体功能：选择一个几何面沿指定方向和长度拉伸为实体，源面保留
 */
class ExtrudeFaceHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;
};
}
