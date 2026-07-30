/**
 * @file MeshAdjacency.h
 * @brief 网格邻接查询索引——统一边表 + 稳定边 id
 *
 * 从 MeshData 派生的查询结构，分两部分：
 *
 * 【懒重建部分】统一边表（invalidate 后按最新拓扑重建）：
 * - 以排序端点对为唯一键，汇聚边数组（先灌，保 cell 顺序）与面单元的边（重合归并）两个数据源；
 * - 行号即当轮 component 局部边序号，供邻接查询；体边暂未纳入，数据源可按同模式追加。
 *
 * 【持久部分】稳定 id 层（invalidate 时保留，跨拓扑编辑有效）：
 * - 端点对 -> 稳定局部边 id（sid）单调分配、不复用，行号重建后经端点对恢复；
 * - sid -> 全局边 id（gid，经 MeshIDMap 分配）在受控点（入池/物化/整网格替换）同步；
 * - 持久层只随 releaseEdgeGlobalIds()（组件移除/整网格替换）重置。
 *   消亡边的 sid/gid 不回收（保留在持久层中成为空洞），待后续 undo/redo 设计定稿再定回收策略。
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

class MeshAdjacency {
public:
    /**
     * @brief 按两端点反查边表行号（当轮 component 局部边序号）
     * @param mesh 目标网格数据（懒构建的数据源）
     * @param p0 边端点 id（与 MeshData 连通性同一键空间，当前为全局点 id）
     * @param p1 边另一端点 id（与 p0 无序）
     * @return 命中返回边表行号；未命中或数据异常返回 std::nullopt
     * @note 行号随拓扑重建重排，跨编辑持有请用 edgeStableId() 换算稳定 id
     */
    std::optional<Index> findEdgeByEndpoints(const MeshData& mesh, Index p0, Index p1);

    /**
     * @brief 边表行号换算物化边 cell 序号
     * @return 该行对应边数组（edge_vertices_）中的 cell 序号；未物化（纯面边）或行号越界返回 -1
     */
    Index edgeCellIndex(const MeshData& mesh, Index edge_row);

    //! @brief 边表行号 -> 稳定局部边 id（跨拓扑编辑有效）
    std::optional<Index> edgeStableId(const MeshData& mesh, Index edge_row);

    //! @brief 稳定局部边 id -> 当轮边表行号；该边已消亡或 id 越界返回 std::nullopt
    std::optional<Index> findEdgeRowByStableId(const MeshData& mesh, Index stable_id);

    //! @brief 稳定局部边 id -> 全局边 id；尚未分配（见 ensureEdgeGlobalIds）或越界返回 std::nullopt
    std::optional<Index> edgeGlobalId(const MeshData& mesh, Index stable_id);

    /**
     * @brief 面单元的边行号表（懒构建）
     * @return 与 MeshData::face_vertices_ 等长对齐的表：face 的第 j 条边
     *         对应 face_edge_rows[face_vertices_offset_[f] + j]（边序按面顶点环绕顺序）
     */
    const std::vector<Index>& faceEdgeRows(const MeshData& mesh);

    //! @brief 边表行数（物化边 + 面边，重合归并后）
    Index edgeCount(const MeshData& mesh);

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

    // —— 懒重建部分（invalidate 后重建）——
    const MeshData* built_mesh_ {}; //> 上次构建所基于的 mesh 对象，指针不同即重建
    bool dirty_ { true }; //> 失效标记
    std::vector<EdgeRow> rows_; //> 边表，行号即当轮局部边序号
    std::unordered_map<std::uint64_t, Index> row_by_endpoints_; //> key=(min<<32)|max，值为行号
    std::vector<Index> face_edge_rows_; //> 与 face_vertices_ 等长，面单元的边行号
    std::vector<Index> row_by_stable_id_; //> 稳定 id -> 行号（-1 空洞：已消亡的边）

    // —— 持久部分（invalidate 时保留，releaseEdgeGlobalIds 才重置）——
    std::unordered_map<std::uint64_t, Index> stable_id_by_endpoints_; //> 端点对 -> 稳定局部边 id
    std::vector<Index> gid_by_stable_id_; //> 稳定局部边 id -> 全局边 id（-1 = 待分配）
};
