// ComponentData.h
#pragma once

#include "Core.h" 
#include "MeshData.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct MeshData;
struct GeometryData;

// 日后在这里补充 Geometry↔网格的映射结构
struct GeometryMeshMap {
    // Geometry 几何边(全局 GeomEdgeId) -> 网格点(全局点池下标)序列
    // 语义：一条 Geometry 边对应一条折线/采样点序列（顺序有意义）
    std::unordered_map<GeomEdgeId, std::vector<Index>> geometry_edge_to_mesh_point_gids;

    void clear() { geometry_edge_to_mesh_point_gids.clear(); }
    bool empty() const { return geometry_edge_to_mesh_point_gids.empty(); }
};

/**
 * @brief 模型的组成单元：一个 ComponentData 表示模型中的一个物体部件
 *
 * - 0/1 个 Geometry 模型（GeometryData）
 * - 0/1 个 网格模型（MeshData）
 * - 将来可挂属性（材料）、Geometry↔网格映射、显示状态等
 */
struct ComponentData {
    Index id { -1 };
    std::string name;

    // 数据部分：一个组件可以有 Geometry，也可以有网格，也可以都有/都没有（初始化阶段）
    std::unique_ptr<MeshData> mesh; ///< 网格数据（来自网格导入或由 Geometry 网格化生成）
    std::unique_ptr<GeometryData> geometry; ///< Geometry 数据（来自 STEP 等）
    std::unique_ptr<GeometryMeshMap> mapping; ///< Geometry↔网格对应关系（可选，后续实现）

    // 组件级属性（后续会扩展 Property/Material）
    Index material_id { -1 }; ///< 材料/属性 ID（先留个 int 占位）
    Index source_xde_leaf_id { -1 };

    ComponentData();
    ~ComponentData();

    // 便捷判断
    bool hasMesh() const noexcept;
    bool hasGeometry() const noexcept;

    MeshData* asMeshData() noexcept { return mesh.get(); }
    const MeshData* asMeshData() const noexcept { return mesh.get(); }

    GeometryData* asGeometryData() noexcept { return geometry.get(); }
    const GeometryData* asGeometryData() const noexcept { return geometry.get(); }

    GeometryMeshMap& ensureMapping();
};