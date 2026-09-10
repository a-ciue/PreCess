#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 创建直线边（坐标）功能：按起点/终点坐标创建直线边，按"写入目标"参数组织结果
 */
class CreateLineByCoordinatesHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;

private:
    int next_line_number_ { 1 }; //> 新建组件时的名称编号（追加既有组件不推进）
};
}
