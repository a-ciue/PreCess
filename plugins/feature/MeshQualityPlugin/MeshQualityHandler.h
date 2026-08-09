/**
 * @file MeshQualityHandler.h
 * @brief 所选组件网格质量计算功能
 */
#ifndef MESH_QUALITY_HANDLER_H
#define MESH_QUALITY_HANDLER_H

#include "FeatureHandler.h"

namespace systems::feature {

/**
 * @brief 计算选择器指定组件的网格质量并生成可渲染的面、体标量属性
 */
class MeshQualityHandler : public FeatureHandler {
public:
    /**
     * @brief 注册目标组件、质量指标参数和功能菜单
     */
    void setup(FeatureRegistrar& reg) override;

    /**
     * @brief 计算质量属性，返回统计文本并通过事件请求显示标量属性
     */
    std::any execute(FeatureContext& ctx) override;
};

}
#endif // MESH_QUALITY_HANDLER_H
