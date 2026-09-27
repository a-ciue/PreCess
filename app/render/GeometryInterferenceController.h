/**
 * @file GeometryInterferenceController.h
 * @brief 同一 Model 内跨 Component 几何干涉的渲染侧编排
 */
#ifndef GEOMETRY_INTERFERENCE_CONTROLLER_H
#define GEOMETRY_INTERFERENCE_CONTROLLER_H

#include "Core.h"

#include <TopoDS_Shape.hxx>

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class GeometryActorManager;

/**
 * @brief 管理模型级几何干涉的归属、缓存、计算触发与日志
 *
 * 单个 GeometryActor 只持有一个 Component，跨 Component 的干涉检查因此由本类
 * 按 Model 编排；模型查询得到的归属、名称和 Shape 在装载时由 Manager 传入。
 */
class GeometryInterferenceController {
public:
    /** @brief 绑定所属 Manager，后续通过它取得 Component Actor 并回填诊断结果。 */
    explicit GeometryInterferenceController(GeometryActorManager& manager);

    /** @brief 记录已装载几何 Component 的 Model 归属、Shape 和 Model 显示标签。 */
    void trackComponent(Index model_id, Index component_id,
        const TopoDS_Shape& shape, std::string model_label);
    /** @brief 返回属于指定 Model 的全部已装载几何 Component。 */
    std::vector<Index> componentIds(Index model_id) const;
    /** @brief 移除 Component 状态，并返回它原先所属的 Model。 */
    std::optional<Index> untrackComponent(Index component_id);
    /** @brief 清理一个 Model 的全部干涉状态。 */
    void removeModel(Index model_id);
    /** @brief 模型几何变更后作废缓存，并按当前开关决定是否重算。 */
    void modelChanged(Index model_id);
    /** @brief 更新几何干涉开关；首次开启时补算所有已装载 Model。 */
    void setEnabled(bool enabled);

private:
    /** @brief 保存干涉计算需要的 Component 归属及 OCC Shape。 */
    struct ComponentGeometry {
        Index model_id { -1 };
        TopoDS_Shape shape;
    };

    /** @brief 重算一个 Model 的几何干涉，并把命中面分发到各 Component。 */
    void rebuild(Index model_id);

    GeometryActorManager& manager_;
    std::unordered_map<Index, ComponentGeometry> components_; //> 已装载 Component 的归属和 Shape
    std::unordered_map<Index, std::string> model_labels_; //> Model 日志显示标签
    std::unordered_set<Index> computed_model_ids_; //> 避免重新开启类别时重复计算
    bool enabled_ { false }; //> 关闭时仅维护归属和缓存失效，不执行计算
};

#endif
