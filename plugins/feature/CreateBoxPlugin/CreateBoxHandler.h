#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 创建长方体功能：按原点与三向尺寸创建 OCC 长方体，按"写入目标"参数组织结果
 *
 * 由 GeometryOperationActions.qml 迁移而来的几何命令功能化试点：
 * 参数经 FeatureSystem 声明链注册（SideBar 依声明渲染），执行走统一的
 * invoke(unique_name) 入口，undo 记录与通知由 invoke 操作边界负责。
 */
class CreateBoxHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;

private:
    int next_box_number_ { 1 }; //> 新建组件时的名称编号（追加既有组件不推进，与原几何命令行为一致）
};
}
