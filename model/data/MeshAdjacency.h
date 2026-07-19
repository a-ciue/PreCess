/**
 * @file MeshAdjacency.h
 * @brief 网格邻接查询索引（派生缓存）
 *
 * 从 MeshData 派生的只读查询结构，懒构建、显式失效：
 * - 查询时若发现 mesh 对象指针变化或已被 invalidate()，自动基于最新拓扑重建；
 * - 拓扑变更后由 ComponentOperator::notifyChanged() 负责失效。
 *
 * 当前仅覆盖独立边（MeshData::edge_vertices_）的端点对反查，
 * 面上的边暂无身份，待后续期次统一边表后纳入。
 */
#pragma once
#include "Core.h"

#include <cstdint>
#include <optional>
#include <unordered_map>

struct MeshData;

class MeshAdjacency {
public:
    /**
     * @brief 按两端点反查独立边的局部 id
     * @param mesh 目标网格数据（懒构建的数据源）
     * @param p0 边端点 id（与 MeshData 连通性同一键空间，当前为全局点 id）
     * @param p1 边另一端点 id（与 p0 无序）
     * @return 命中返回 component 局部边 id；未命中或数据异常返回 std::nullopt
     */
    std::optional<Index> findEdgeByEndpoints(const MeshData& mesh, Index p0, Index p1);

    //! @brief 使索引失效，下次查询时基于最新拓扑重建
    void invalidate() noexcept;

private:
    void ensureBuilt(const MeshData& mesh);

    const MeshData* built_mesh_ {}; //> 上次构建所基于的 mesh 对象，指针不同即重建
    bool dirty_ { true }; //> 失效标记
    std::unordered_map<std::uint64_t, Index> edge_by_endpoints_; //> key=(min<<32)|max，值为局部边 id
};
