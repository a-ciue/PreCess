/**
 * @file MeshAdjacency.h
 * @brief 网格邻接查询索引——统一边表 + 稳定边 id
 *
 * 从 MeshData 派生的查询结构，分两部分：
 *
 * 【懒重建部分】统一边表（invalidate 后按最新拓扑重建）：
 * - 以排序端点对为唯一键，汇聚边数组（先灌，保 cell 顺序）与面单元的边（重合归并）两个数据源；
 * - 表内位置（行号）仅以不透明句柄 EdgeHandle 对外：不提供到 id 的转换、不提供比较运算，
 *   只能回传给 MeshAdjacency 消费，边表每次重建后失效；体边暂未纳入，数据源可按同模式追加。
 *
 * 【持久部分】稳定 id 层（invalidate 时保留，跨拓扑编辑有效）：
 * - 端点对 -> 稳定局部边 id（sid）单调分配、不复用，是跨层传递的公共身份（Selection 等携带）；
 * - sid -> 全局边 id（gid，经 MeshIDMap 分配）在受控点（入池/物化/整网格替换）同步；
 * - 持久层只随 releaseEdgeGlobalIds()（组件移除/整网格替换）重置。
 *   消亡边的 sid/gid 不回收（保留为空洞），回收策略随 undo/redo 设计定稿。
 */
#pragma once
#include "Core.h"

#include <array>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

struct MeshData;
class MeshIDMap;

/**
 * @brief 边表行号的不透明句柄（仅当轮边表有效）
 *
 * 仅由 MeshAdjacency 签发、并回传给 MeshAdjacency 消费的临时对象：
 * 不提供到 id 的转换、不提供比较运算，防止把当轮行号误作稳定 id 持有或使用。
 * 需要跨拓扑编辑持有时，经 MeshAdjacency::edgeStableId 换算稳定局部边 id。
 */
class EdgeHandle {
    friend class MeshAdjacency;

    EdgeHandle(Index row, std::uint32_t generation) noexcept
        : row_(row)
        , generation_(generation)
    {
    }

    Index row_ { -1 }; //> 当轮边表行号
    std::uint32_t generation_ { 0 }; //> 签发时的边表构建世代（重建即失效）

public:
    EdgeHandle() = default;
};

class MeshAdjacency {
public:
    // —— 稳定 id 接口（公共契约，跨拓扑编辑有效）——

    //! @brief 边句柄 -> 稳定局部边 id；句柄无效（含边表已重建）返回 std::nullopt
    std::optional<Index> edgeStableId(const MeshData& mesh, EdgeHandle edge);

    //! @brief 稳定局部边 id -> 全局边 id；尚未分配（见 ensureEdgeGlobalIds）或越界返回 std::nullopt
    std::optional<Index> edgeGlobalId(const MeshData& mesh, Index stable_id);

    /**
     * @brief 面单元的边稳定 id 表（懒构建）
     * @return 与 MeshData::face_vertices_ 等长对齐的表：face 的第 j 条边
     *         对应 faceEdgeStableIds[face_vertices_offset_[f] + j]（边序按面顶点环绕顺序）
     */
    const std::vector<Index>& faceEdgeStableIds(const MeshData& mesh);

    // —— 句柄签发与消费（当轮操作工具，EdgeHandle 只能经 MeshAdjacency 进出）——

    /**
     * @brief 按两端点反查边句柄
     * @param mesh 目标网格数据（懒构建的数据源）
     * @param p0 边端点 id（与 MeshData 连通性同一键空间，当前为全局点 id）
     * @param p1 边另一端点 id（与 p0 无序）
     * @return 命中返回边句柄；未命中或数据异常返回 std::nullopt
     */
    std::optional<EdgeHandle> findEdgeByEndpoints(const MeshData& mesh, Index p0, Index p1);

    //! @brief 稳定局部边 id -> 边句柄；该边已消亡或 id 越界返回 std::nullopt
    std::optional<EdgeHandle> findEdgeByStableId(const MeshData& mesh, Index stable_id);

    //! @brief 边句柄 -> 物化边 cell 序号；未物化（纯面边）或句柄无效返回 -1
    Index edgeCellIndex(const MeshData& mesh, EdgeHandle edge);

    //! @brief 当轮边表行数（物化边 + 面边，重合归并后）
    Index edgeCount(const MeshData& mesh);

    // —— 全局 id 同步与生命周期 ——

    /**
     * @brief 为全部稳定 id 分配全局边 id（幂等，仅补缺）
     *
     * 在受控点调用：模型入池（ModelLayer::addModel）、整网格替换、物化操作。
     * gid -> (component_id, sid) 写入 MeshIDMap。
     */
    void ensureEdgeGlobalIds(MeshIDMap& map, Index component_id, const MeshData& mesh);

    //! @brief 释放全部全局边 id 并重置持久身份层（组件移除/整网格替换时调用）
    void releaseEdgeGlobalIds(MeshIDMap& map);

    //! @brief 使懒重建部分失效，下次查询时基于最新拓扑重建（持久稳定 id 层保留）
    void invalidate() noexcept;

private:
    struct EdgeRow {
        std::array<Index, 2> endpoints; //> 排序端点对（小在前），唯一键
        Index cell_index { -1 }; //> 物化边在 edge_vertices_ 中的 cell 序号，-1 表示纯面边
        Index stable_id { -1 }; //> 稳定局部边 id
    };

    void ensureBuilt(const MeshData& mesh);
    //! @brief 句柄是否属于当轮边表（世代匹配且行号在界内）
    bool isCurrent(EdgeHandle edge) const noexcept;

    // —— 懒重建部分（invalidate 后重建）——
    const MeshData* built_mesh_ {}; //> 上次构建所基于的 mesh 对象，指针不同即重建
    bool dirty_ { true }; //> 失效标记
    std::uint32_t generation_ { 0 }; //> 边表构建世代，每次重建递增
    std::vector<EdgeRow> rows_; //> 边表
    std::unordered_map<std::uint64_t, Index> row_by_endpoints_; //> key=(min<<32)|max，值为行号
    std::vector<Index> face_edge_stable_ids_; //> 与 face_vertices_ 等长，面单元的边稳定 id
    std::vector<Index> row_by_stable_id_; //> 稳定 id -> 行号（-1 空洞：已消亡的边）

    // —— 持久部分（invalidate 时保留，releaseEdgeGlobalIds 才重置）——
    std::unordered_map<std::uint64_t, Index> stable_id_by_endpoints_; //> 端点对 -> 稳定局部边 id
    std::vector<Index> gid_by_stable_id_; //> 稳定局部边 id -> 全局边 id（-1 = 待分配）
};
