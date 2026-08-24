/**
 * @file MeshQualityHandler.h
 * @brief 所选组件网格质量计算功能
 */
#ifndef MESH_QUALITY_HANDLER_H
#define MESH_QUALITY_HANDLER_H

#include "ComponentOperator.h"
#include "Core.h"
#include "FeatureHandler.h"

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace systems::feature {

/**
 * @brief 计算选择器指定组件的网格质量并生成可渲染的面、体标量属性
 *
 * 生成的质量属性随功能退出（deactivate）清理，注销（teardown）兜底；
 * 属性显示请求经 ScalarAttributeDisplayRequestedEvent 通知界面（界面在活动操作
 * 切换时自行取消渲染，见 CentralRenderArea）。
 */
class MeshQualityHandler : public FeatureHandler {
public:
    /**
     * @brief 注册目标组件、质量指标参数和功能菜单
     */
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;

    /**
     * @brief 功能退出：删除本次操作生成的全部质量属性
     */
    void deactivate(FeatureContext& ctx) override;

    /**
     * @brief 注销兜底：退出路径已清理，此处仅容错空转
     */
    void teardown(FeatureContext& ctx) override;

    /**
     * @brief 计算质量属性，返回统计文本并通过事件请求显示标量属性
     */
    std::any execute(FeatureContext& ctx) override;

private:
    /**
     * @brief 根据 Component ID 申请写操作句柄的上下文函数
     */
    using ComponentOperatorProvider = std::function<std::optional<ComponentOperator>(Index)>;

    /**
     * @brief 单个组件在当前操作中生成的面、体质量属性名
     */
    struct GeneratedAttributes {
        std::vector<std::string> face_names;
        std::vector<std::string> solid_names;
    };

    /**
     * @brief 删除当前操作生成的全部质量属性
     * @param component_operator Component 写操作句柄申请函数
     */
    void clearGeneratedAttributes(const ComponentOperatorProvider& component_operator);

    std::map<Index, GeneratedAttributes> generated_attributes_; //> 按组件记录当前操作生成的质量属性
};

}
#endif // MESH_QUALITY_HANDLER_H
