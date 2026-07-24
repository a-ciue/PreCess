/**
 * @file MeshQualityHandler.h
 * @brief 当前组件网格质量计算功能
 */
#ifndef MESH_QUALITY_HANDLER_H
#define MESH_QUALITY_HANDLER_H

#include "FeatureHandler.h"

namespace systems::feature {

/**
 * @brief 计算当前组件的网格质量并生成可渲染的面、体标量属性
 */
class MeshQualityHandler : public FeatureHandler {
public:
    /**
     * @brief 注册质量指标参数和功能菜单
     */
    void setup(FeatureRegistrar& reg) override;

    /**
     * @brief 计算质量属性，并将统计文本和待显示属性名返回给 UI
     */
    std::any execute(FeatureContext& ctx) override;
};

}
#endif // MESH_QUALITY_HANDLER_H
