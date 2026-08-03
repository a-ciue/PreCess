/**
 * @file MeshQualityHandler.h
 * @brief 当前组件网格质量计算功能
 */
#ifndef MESH_QUALITY_HANDLER_H
#define MESH_QUALITY_HANDLER_H

#include "Core.h"
#include "EventBus.h"
#include "FeatureHandler.h"

#include <map>
#include <string>
#include <vector>

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
     * @brief 订阅属性显示事件，在活动操作切换时清理生成的质量属性
     */
    void activate(FeatureContext& ctx) override;

    /**
     * @brief 计算质量属性，并将统计文本和待显示属性名返回给 UI
     */
    std::any execute(FeatureContext& ctx) override;

private:
    /**
     * @brief 单个组件在当前操作中生成的面、体质量属性名
     */
    struct GeneratedAttributes {
        std::vector<std::string> face_names;
        std::vector<std::string> solid_names;
    };

    /**
     * @brief 删除当前操作生成的全部质量属性并通知模型刷新
     */
    void clearGeneratedAttributes(FeatureContext& ctx);

    std::map<Index, GeneratedAttributes> generated_attributes_; //> 按组件记录当前操作生成的质量属性
    core::EventBus::Subscription attribute_display_sub_; //> 属性显示事件订阅句柄
};

}
#endif // MESH_QUALITY_HANDLER_H
