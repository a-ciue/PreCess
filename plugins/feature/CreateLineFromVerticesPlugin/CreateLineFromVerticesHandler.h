#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 创建直线边（选择两点）功能：选择组件中两个已有几何点创建共享拓扑的直线边
 */
class CreateLineFromVerticesHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;

private:
    int next_line_number_ { 1 }; //> 新建直线的名称编号
};
}
