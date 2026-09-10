#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 创建圆锥/圆台功能：按底面圆心、底/顶半径、高度、轴向与扫掠角创建圆锥或圆台
 */
class CreateConeHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;

private:
    int next_cone_number_ { 1 }; //> 新建组件时的名称编号（追加既有组件不推进）
};
}
