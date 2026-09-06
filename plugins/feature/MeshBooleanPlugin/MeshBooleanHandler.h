/**
 * @file MeshBooleanHandler.h
 * @brief 网格布尔功能处理器：基于 CGAL PMP corefinement 的交/并/差运算
 */
#ifndef MESH_BOOLEAN_HANDLER_H
#define MESH_BOOLEAN_HANDLER_H

#include "ComponentOperator.h"
#include "Core.h"
#include "FeatureHandler.h"

#include <any>

namespace systems::feature {

/**
 * @brief 基于 CGAL Polygon Mesh Processing corefinement 的网格布尔功能
 *
 * 支持并集 / 交集 / 差集（A−B）/ 差集（B−A）四种运算；对象 A 与对象 B 均由
 * Selector 参数显式选择，结果以**新模型**的形式加入模型层（ModelLayer::addModel），
 * 两个操作对象都保持原样，便于与原对象对比、单独导出或删除。
 * 内部使用 EPECK 精确内核（相交点精确构造），避免 EPIC 浮点舍入导致的结果错误。
 *
 * 前提约束（违反时返回温和中文提示，不进入 CGAL）：
 *   1) 两对象均为纯三角表面网格（无体单元、无非三角面）；
 *   2) 两对象均为闭合（水密）网格；
 *   3) 两对象均无自相交面；
 *   4) 两个 Component 必须不同。
 * 两表面不相交时的退化场景（互相包含 / 相互分离）按体积语义给出明确结果与提示。
 */
class MeshBooleanHandler : public FeatureHandler {
public:
    /**
     * @brief 注册对象 A/B 两个 Selector 参数、运算类型 Combo 参数与功能菜单
     */
    void setup(FeatureRegistrar& reg, FeatureContext&) override;

    /**
     * @brief 执行所选布尔运算并返回结果文本
     */
    std::any execute(FeatureContext& ctx) override;
};

} // namespace systems::feature

#endif // MESH_BOOLEAN_HANDLER_H
