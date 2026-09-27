/**
 * @file GeometryInterferenceController.h
 * @brief 同一 Model 内跨 Component 几何干涉的渲染侧编排
 */
#ifndef GEOMETRY_INTERFERENCE_CONTROLLER_H
#define GEOMETRY_INTERFERENCE_CONTROLLER_H

#include "Core.h"

#include <unordered_map>
#include <unordered_set>

class GeometryActorManager;
class QModelQuery;

/**
 * @brief 管理模型级几何干涉的归属、缓存、计算触发与日志
 *
 * 单个 GeometryActor 只持有一个 Component，跨 Component 的干涉检查因此由本类
 * 按 Model 编排；GeometryActorManager 仍只负责 Actor 与类别开关。
 */
class GeometryInterferenceController {
public:
    /** @brief 记录已装载几何 Component 的 Model 归属。 */
    void trackComponent(Index model_id, Index component_id);
    /** @brief 删除 Model 的几何 Actor，并清理对应干涉状态。 */
    void removeModel(Index model_id, GeometryActorManager& manager);
    /** @brief 删除 Component，并在诊断开启时刷新其原所属 Model。 */
    void removeComponent(
        Index component_id, GeometryActorManager& manager, QModelQuery& query);
    /** @brief 模型几何变更后作废缓存，并按当前开关决定是否重算。 */
    void modelChanged(Index model_id, GeometryActorManager& manager, QModelQuery& query);
    /** @brief 更新几何干涉开关；首次开启时补算所有已装载 Model。 */
    void setEnabled(bool enabled, GeometryActorManager& manager, QModelQuery& query);

private:
    /** @brief 重算一个 Model 的几何干涉，并把命中面分发到各 Component。 */
    void rebuild(Index model_id, GeometryActorManager& manager, QModelQuery& query);

    std::unordered_map<Index, Index> component_model_ids_; //> 删除事件使用的 Component 归属
    std::unordered_set<Index> computed_model_ids_; //> 避免重新开启类别时重复计算
    bool enabled_ { false }; //> 关闭时仅维护归属和缓存失效，不执行计算
};

#endif
