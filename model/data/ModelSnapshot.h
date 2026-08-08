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
 *       已知限制：几何子形状索引重建时领新 gid（索引是派生缓存，GeometryRegistry 发号
 *       无法 reclaim 原值），几何 gid 身份跨 undo 不保持；mesh 侧点/边 gid 身份由 reclaim 保证。
 */
struct ModelSnapshot {
    Index model_id { -1 };
    std::string name;
    ComponentDatas components; //> 各组件 clone，id 为原值
};
