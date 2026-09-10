#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 创建圆盘/扇形面功能：按圆心、半径、坐标平面与角度创建圆盘或扇形面
 */
class CreateDiskFaceHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;

private:
    int next_disk_face_number_ { 1 }; //> 新建组件时的名称编号（追加既有组件不推进）
};
}
