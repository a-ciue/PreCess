// ComponentData.h
#pragma once

#include "Core.h" 
#include "MeshData.h"
#include "MeshAdjacency.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct MeshData;
struct GeometryData;
class MeshIDMap;

struct ComponentData;
using ComponentDatas = std::vector<std::unique_ptr<ComponentData>>;

// 日后在这里补充 Geometry↔网格的映射结构
struct GeometryMeshMap {
    // Geometry 几何边(全局 GeomEdgeId) -> 网格点(组件内局部点 id)序列
    // 语义：一条 Geometry 边对应一条折线/采样点序列（顺序有意义）
    std::unordered_map<GeomEdgeId, std::vector<Index>> geometry_edge_to_mesh_point_ids;

    void clear() { geometry_edge_to_mesh_point_ids.clear(); }
    bool empty() const { return geometry_edge_to_mesh_point_ids.empty(); }
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

    MeshAdjacency mesh_adjacency; ///< 网格邻接查询索引（派生缓存，拓扑变更后由 ComponentOperator::notifyChanged 失效）

    /**
     * @brief 局部点 id -> 全局点 id（gid 经 ModelLayer::pointIdMap() 分配，-1 = 待分配）
     *
     * 组件级伴生身份数据（与 mesh_adjacency 的全局边 id 层同机制），不属于 MeshData，
     * 保证 MeshData 可独立快照/恢复。仅在受控点同步：模型入池（ModelLayer::addModel）、
     * 整网格替换、运行期向 mesh 加点后（ensurePointGlobalIds）、组件/网格移除（releasePointGlobalIds）。
     * gid 复用策略同 MeshIDMap（free-list 保留），快照恢复经 reclaim 按原值定向拿回 gid；
     * undo 后选择集清空（Selection 持有的 gid/稳定 id 不作跨 undo 保证）。
     */
    std::vector<Index> point_global_ids_;

    // 组件级属性（后续会扩展 Property/Material）
    Index material_id { -1 }; ///< 材料/属性 ID（先留个 int 占位）
    Index source_xde_leaf_id { -1 };

    ComponentData();
    ~ComponentData();

    //! @brief 深拷贝整个组件（mesh/geometry/mapping/gid 伴生表/邻接持久层），id 一并复制
    std::unique_ptr<ComponentData> clone() const;

    /**
     * @brief 用快照覆盖本组件数据（保留本组件 id 不变）
     * @note 不触碰 ModelLayer 的 MeshIDMap；gid 对账由 ComponentOperator::restoreSnapshot 负责。
     *       mesh_adjacency 拷贝只带持久身份层，懒表自动置 dirty（见 MeshAdjacency 拷贝语义）。
     */
    void restoreFrom(const ComponentData& snapshot);

    //! @brief 按 point_global_ids_ 原值定向回收点 gid（配合 restoreFrom 使用）
    void reclaimPointGlobalIds(MeshIDMap& map);

    // 便捷判断
    bool hasMesh() const noexcept;
    bool hasGeometry() const noexcept;

    MeshData* asMeshData() noexcept { return mesh.get(); }
    const MeshData* asMeshData() const noexcept { return mesh.get(); }

    GeometryData* asGeometryData() noexcept { return geometry.get(); }
    const GeometryData* asGeometryData() const noexcept { return geometry.get(); }

    GeometryMeshMap& ensureMapping();

    /**
     * @brief 为全部局部点分配全局点 id（幂等，仅补缺）
     *
     * 在受控点调用：模型入池（ModelLayer::addModel）、整网格替换、运行期加点后。
     * gid -> (component_id, 局部点 id) 写入 MeshIDMap。
     */
    void ensurePointGlobalIds(MeshIDMap& map);

    //! @brief 释放全部全局点 id 并清空映射（组件移除/整网格替换/网格移除时调用）
    void releasePointGlobalIds(MeshIDMap& map);
};