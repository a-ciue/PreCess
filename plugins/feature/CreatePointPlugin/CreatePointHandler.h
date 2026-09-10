#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 创建点功能：按三维坐标创建独立几何点，按"写入目标"参数组织结果
 */
class CreatePointHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;

private:
    int next_point_number_ { 1 }; //> 新建组件时的名称编号（追加既有组件不推进）
};
}
