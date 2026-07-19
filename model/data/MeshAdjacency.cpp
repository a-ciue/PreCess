#include "MeshAdjacency.h"

#include "MeshData.h"

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

    auto it = edge_by_endpoints_.find(packEndpoints(p0, p1));
    if (it == edge_by_endpoints_.end())
        return std::nullopt;
    return it->second;
}

void MeshAdjacency::invalidate() noexcept
{
    dirty_ = true;
}

void MeshAdjacency::ensureBuilt(const MeshData& mesh)
{
    if (!dirty_ && built_mesh_ == &mesh)
        return;

    edge_by_endpoints_.clear();
    built_mesh_ = &mesh;
    dirty_ = false;

    const auto& edges = mesh.edge_vertices_;
    if (edges.size() % 2 != 0) {
        // 异常数据按无有效边处理，与 MeshData::ensureEdgeIdMapBuilt 的错误口径一致
        spdlog::error("MeshAdjacency::ensureBuilt: edge_vertices_ size is odd ({})", edges.size());
        return;
    }

    const Index edge_count = static_cast<Index>(edges.size() / 2);
    for (Index eid = 0; eid < edge_count; ++eid) {
        // 同一端点对存在多条边时保留先见者
        edge_by_endpoints_.try_emplace(packEndpoints(edges[2 * eid], edges[2 * eid + 1]), eid);
    }
}
