#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 创建圆柱体功能：按底面圆心、半径、高度、轴向与扫掠角创建圆柱体
 */
class CreateCylinderHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;

private:
    int next_cylinder_number_ { 1 }; //> 新建组件时的名称编号（追加既有组件不推进）
};
}
