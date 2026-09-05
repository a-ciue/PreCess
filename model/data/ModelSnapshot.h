/**
 * @file ModelSnapshot.h
 * @brief 模型级结构快照（undo/redo 原语）
 */
#pragma once
#include "ComponentData.h"

#include <string>

/**
 * @brief 模型级结构快照：整模型的深拷贝，用于撤销/恢复结构操作
 *
 * @note 携带原 model_id/component_id；恢复时按原 id 插回（发号器只增不减，不会撞号）。
 *       点/边 gid 经 reclaim 按原值拿回；几何 gid 向量随克隆保留、索引重建时按原值
 *       reclaim（GeometryRegistry 发号只增不复用），gid 身份跨 undo 保持。
 */
struct ModelSnapshot {
    Index model_id { -1 };
    std::string name;
    ComponentDatas components; //> 各组件 clone，id 为原值
};
