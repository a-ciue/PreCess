#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 选择闭合边创建面功能：选择组件中组成单一闭合轮廓的几何边创建面
 */
class CreateFaceFromEdgesHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;
};
}
