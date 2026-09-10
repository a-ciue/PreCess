#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 创建矩形面功能：按角点、宽高与全局坐标平面创建矩形面，按"写入目标"参数组织结果
 */
class CreateRectangleFaceHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;

private:
    int next_rectangle_face_number_ { 1 }; //> 新建组件时的名称编号（追加既有组件不推进）
};
}
