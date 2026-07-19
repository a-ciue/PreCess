/**
 * @file MeshAdjacency.h
 * @brief 网格邻接查询索引（派生缓存）——统一边表
 *
 * 从 MeshData 派生的只读查询结构，懒构建、显式失效：
 * - 查询时若发现 mesh 对象指针变化或已被 invalidate()，自动基于最新拓扑重建；
 * - 拓扑变更后由 ComponentOperator::notifyChanged() 负责失效。
 *
 * 统一边表：以排序端点对为唯一键，汇聚两个数据源——
 *   1. 边数组（MeshData::edge_vertices_，物化边：自由边/挂属性的边），先灌入以保 cell 顺序；
 *   2. 面单元的边（MeshData::face_vertices_ 展开），重合的边归并到已有行。
 * 表行号即 component 局部边 id（所有几何边都有身份）；物化信息（cell_index）与身份分离，
 * 边数组与边属性（按 cell 序号索引）的存量语义不变。
 * 体边暂未纳入（与"体的面/边先不做"一致），数据源可按同模式追加。
 */
#pragma once
#include "Core.h"

#include <array>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

struct MeshData;

class MeshAdjacency {
public:
    /**
     * @brief 按两端点反查边表行号（component 局部边 id）
     * @param mesh 目标网格数据（懒构建的数据源）
     * @param p0 边端点 id（与 MeshData 连通性同一键空间，当前为全局点 id）
     * @param p1 边另一端点 id（与 p0 无序）
     * @return 命中返回边表行号；未命中或数据异常返回 std::nullopt
     * @note 返回的是统一边表行号，不是边数组 cell 序号；cell 序号用 edgeCellIndex() 换算
     */
    std::optional<Index> findEdgeByEndpoints(const MeshData& mesh, Index p0, Index p1);

    /**
     * @brief 边表行号换算物化边 cell 序号
     * @param mesh 目标网格数据
     * @param edge_row 边表行号
     * @return 该行对应边数组（edge_vertices_）中的 cell 序号；未物化（纯面边）返回 -1，行号越界返回 -1
     */
    Index edgeCellIndex(const MeshData& mesh, Index edge_row);

    /**
     * @brief 面单元的边行号表（懒构建）
     * @param mesh 目标网格数据
     * @return 与 MeshData::face_vertices_ 等长对齐的表：face 的第 j 条边
     *         对应 face_edge_rows[face_vertices_offset_[f] + j]（边序按面顶点环绕顺序）
     */
    const std::vector<Index>& faceEdgeRows(const MeshData& mesh);

    //! @brief 边表行数（物化边 + 面边，重合归并后）
    Index edgeCount(const MeshData& mesh);

    //! @brief 使索引失效，下次查询时基于最新拓扑重建
    void invalidate() noexcept;

private:
    struct EdgeRow {
        std::array<Index, 2> endpoints; //> 排序端点对（小在前），唯一键
        Index cell_index { -1 }; //> 物化边在 edge_vertices_ 中的 cell 序号，-1 表示纯面边
    };

    void ensureBuilt(const MeshData& mesh);

    const MeshData* built_mesh_ {}; //> 上次构建所基于的 mesh 对象，指针不同即重建
    bool dirty_ { true }; //> 失效标记
    std::vector<EdgeRow> rows_; //> 边表，行号即局部边 id
    std::unordered_map<std::uint64_t, Index> row_by_endpoints_; //> key=(min<<32)|max，值为行号
    std::vector<Index> face_edge_rows_; //> 与 face_vertices_ 等长，面单元的边行号
};
