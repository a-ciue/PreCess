/**
 * @file MeshRepairHandler.h
 * @brief 网格修复功能处理器：基于 CGAL Polygon Mesh Processing 的补洞、自交检测与退化清理
 */
#ifndef MESH_REPAIR_HANDLER_H
#define MESH_REPAIR_HANDLER_H

#include "ComponentOperator.h"
#include "Core.h"
#include "FeatureHandler.h"

#include <any>

namespace systems::feature {

/**
 * @brief 基于 CGAL PMP 的网格修复功能
 *
 * 三种操作通过 Combo 参数选择：孔洞填补（三角化边界环）、自相交面检测、退化面清理。
 * 目标 Component 由 Selector 参数显式选择（参数 0），避免依赖对象树选中态。
 * 写路径经 ComponentOperator::replaceMesh 由 FeatureSystem::invoke 边界统一 flush 通知。
 */
class MeshRepairHandler : public FeatureHandler {
public:
    /**
     * @brief 注册目标 Component 选择器、操作类型参数与功能菜单
     */
    void setup(FeatureRegistrar& reg) override;

    /**
     * @brief 执行所选修复操作并返回结果文本
     */
    std::any execute(FeatureContext& ctx) override;
};

} // namespace systems::feature

#endif // MESH_REPAIR_HANDLER_H