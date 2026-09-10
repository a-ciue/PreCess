#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 创建球体/部分球体功能：按球心、半径、极轴、纬度范围与经度扫掠角创建球体
 */
class CreateSphereHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;

private:
    int next_sphere_number_ { 1 }; //> 新建组件时的名称编号（追加既有组件不推进）
};
}
