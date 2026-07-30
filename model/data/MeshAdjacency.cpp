#include "MeshAdjacency.h"

#include "MeshData.h"
#include "MeshIDMap.h"

#include <spdlog/spdlog.h>

namespace {
//! @brief 端点对打包为无序键：小端点占低 32 位，大端点占高 32 位
std::uint64_t packEndpoints(Index p0, Index p1)
{
    const std::uint32_t lo = static_cast<std::uint32_t>(p0 < p1 ? p0 : p1);
    const std::uint32_t hi = static_cast<std::uint32_t>(p0 < p1 ? p1 : p0);
    return (static_cast<std::uint64_t>(hi) << 32) | lo;
}
}

std::optional<Index> MeshAdjacency::findEdgeByEndpoints(const MeshData& mesh, Index p0, Index p1)
{
    if (p0 < 0 || p1 < 0)
        return std::nullopt;

    ensureBuilt(mesh);

    auto it = row_by_endpoints_.find(packEndpoints(p0, p1));
    if (it == row_by_endpoints_.end())
        return std::nullopt;
    return it->second;
}

Index MeshAdjacency::edgeCellIndex(const MeshData& mesh, Index edge_row)
{
    ensureBuilt(mesh);

    if (edge_row < 0 || edge_row >= static_cast<Index>(rows_.size()))
        return -1;
    return rows_[edge_row].cell_index;
}

std::optional<Index> MeshAdjacency::edgeStableId(const MeshData& mesh, Index edge_row)
{
    ensureBuilt(mesh);

    if (edge_row < 0 || edge_row >= static_cast<Index>(rows_.size()))
        return std::nullopt;
    return rows_[edge_row].stable_id;
}

std::optional<Index> MeshAdjacency::findEdgeRowByStableId(const MeshData& mesh, Index stable_id)
{
    ensureBuilt(mesh);

    if (stable_id < 0 || stable_id >= static_cast<Index>(row_by_stable_id_.size()))
        return std::nullopt;
    const Index row = row_by_stable_id_[stable_id];
    if (row < 0) // 该边在当前拓扑中已消亡
        return std::nullopt;
    return row;
}

std::optional<Index> MeshAdjacency::edgeGlobalId(const MeshData& mesh, Index stable_id)
{
    ensureBuilt(mesh);

    if (stable_id < 0 || stable_id >= static_cast<Index>(gid_by_stable_id_.size()))
        return std::nullopt;
    const Index gid = gid_by_stable_id_[stable_id];
    if (gid < 0) // 尚未经 ensureEdgeGlobalIds 分配
        return std::nullopt;
    return gid;
}

const std::vector<Index>& MeshAdjacency::faceEdgeRows(const MeshData& mesh)
{
    ensureBuilt(mesh);
    return face_edge_rows_;
}

Index MeshAdjacency::edgeCount(const MeshData& mesh)
{
    ensureBuilt(mesh);
    return static_cast<Index>(rows_.size());
}

void MeshAdjacency::ensureEdgeGlobalIds(MeshIDMap& map, Index component_id, const MeshData& mesh)
{
    ensureBuilt(mesh);

    // 幂等补缺：只为尚未分配的稳定 id 申请 gid
    for (Index sid = 0; sid < static_cast<Index>(gid_by_stable_id_.size()); ++sid) {
        if (gid_by_stable_id_[sid] < 0)
            gid_by_stable_id_[sid] = map.insert(component_id, sid);
    }
}

void MeshAdjacency::releaseEdgeGlobalIds(MeshIDMap& map)
{
    for (Index gid : gid_by_stable_id_) {
        if (gid >= 0)
            map.remove(gid);
    }
    gid_by_stable_id_.clear();
    stable_id_by_endpoints_.clear();
    invalidate();
}

void MeshAdjacency::invalidate() noexcept
{
    dirty_ = true;
}

void MeshAdjacency::ensureBuilt(const MeshData& mesh)
{
    if (!dirty_ && built_mesh_ == &mesh)
        return;

    rows_.clear();
    row_by_endpoints_.clear();
    face_edge_rows_.clear();
    built_mesh_ = &mesh;
    dirty_ = false;

    // 端点对归并建行：持久稳定 id + 当轮行号。返回行号。
    auto resolve_row = [&](Index p0, Index p1) -> Index {
        const std::uint64_t key = packEndpoints(p0, p1);

        // 持久身份：首次见到的端点对单调分配稳定 id（不复用、不回收，见文件头说明）
        Index sid;
        if (auto sid_it = stable_id_by_endpoints_.find(key); sid_it != stable_id_by_endpoints_.end()) {
            sid = sid_it->second;
        } else {
            sid = static_cast<Index>(gid_by_stable_id_.size());
            stable_id_by_endpoints_.emplace(key, sid);
            gid_by_stable_id_.push_back(-1);
        }

        // 当轮边表行
        auto [it, inserted] = row_by_endpoints_.try_emplace(key, static_cast<Index>(rows_.size()));
        if (inserted) {
            EdgeRow row;
            row.endpoints = { p0 < p1 ? p0 : p1, p0 < p1 ? p1 : p0 };
            row.stable_id = sid;
            rows_.push_back(row);
        }
        return it->second;
    };

    // 源一：边数组（物化边）。先灌入以保 cell 顺序，重复边合并到先见行
    const auto& edge_vertices = mesh.edge_vertices_;
    if (edge_vertices.size() % 2 != 0) {
        // 异常数据跳过该源（按无有效边处理）
        spdlog::error("MeshAdjacency::ensureBuilt: edge_vertices_ size is odd ({})", edge_vertices.size());
    } else {
        const Index cell_count = static_cast<Index>(edge_vertices.size() / 2);
        for (Index cell = 0; cell < cell_count; ++cell) {
            const Index p0 = edge_vertices[2 * cell];
            const Index p1 = edge_vertices[2 * cell + 1];
            if (p0 < 0 || p1 < 0)
                continue;

            const Index row_id = resolve_row(p0, p1);
            auto& row = rows_[row_id];
            if (row.cell_index < 0) {
                row.cell_index = cell;
            } else {
                spdlog::warn("MeshAdjacency::ensureBuilt: duplicated edge cell {} on endpoints ({}, {}), first cell {} kept",
                    cell, p0, p1, row.cell_index);
            }
        }
    }

    // 源二：面单元的边（含与物化边重合者，经 resolve_row 自动归并）
    const auto& face_vertices = mesh.face_vertices_;
    const auto& offsets = mesh.face_vertices_offset_;
    if (offsets.size() < 2) {
        // 无面单元，仍要回填 row_by_stable_id_
    } else {
        face_edge_rows_.assign(face_vertices.size(), -1);
        const Index face_count = static_cast<Index>(offsets.size() - 1);
        for (Index f = 0; f < face_count; ++f) {
            const Index begin = offsets[f];
            const Index n = offsets[f + 1] - begin;
            if (n < 2) // 无法成边的退化面
                continue;

            // 面顶点环绕展开边：(v0,v1), (v1,v2), ..., (v_{n-1},v0)
            for (Index j = 0; j < n; ++j) {
                const Index p0 = face_vertices[begin + j];
                const Index p1 = face_vertices[begin + (j + 1) % n];
                if (p0 < 0 || p1 < 0)
                    continue;
                face_edge_rows_[begin + j] = resolve_row(p0, p1);
            }
        }
    }

    // 回填稳定 id -> 行号（消亡边留 -1 空洞）
    row_by_stable_id_.assign(gid_by_stable_id_.size(), -1);
    for (Index r = 0; r < static_cast<Index>(rows_.size()); ++r)
        row_by_stable_id_[rows_[r].stable_id] = r;
}
