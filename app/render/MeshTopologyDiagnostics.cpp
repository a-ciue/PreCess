#include "MeshTopologyDiagnostics.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace {
/**
 * @brief 诊断计算使用的临时边拓扑
 */
struct MeshEdgeTopology {
    std::array<Index, 2> endpoints; //> 排序后的组件内局部点 id
    std::vector<Index> adjacent_faces; //> 使用该边的面单元 id
};

//! @brief 端点对打包为无序键：小端点占低 32 位，大端点占高 32 位
std::uint64_t packEndpoints(Index p0, Index p1)
{
    const std::uint32_t lo = static_cast<std::uint32_t>(p0 < p1 ? p0 : p1);
    const std::uint32_t hi = static_cast<std::uint32_t>(p0 < p1 ? p1 : p0);
    return (static_cast<std::uint64_t>(hi) << 32) | lo;
}

//! @brief 从显式边和面边构建仅供本次诊断使用的临时边表
std::vector<MeshEdgeTopology> buildEdgeTopologies(const MeshDataVtk& mesh)
{
    std::vector<MeshEdgeTopology> edges;
    std::unordered_map<std::uint64_t, size_t> edge_by_endpoints;

    auto resolve_edge = [&](Index p0, Index p1) -> MeshEdgeTopology& {
        const std::uint64_t key = packEndpoints(p0, p1);
        auto [it, inserted] = edge_by_endpoints.try_emplace(key, edges.size());
        if (inserted)
            edges.push_back({ { std::min(p0, p1), std::max(p0, p1) }, {} });
        return edges[it->second];
    };

    // 显式边先进入边表，没有相邻面时将被识别为孤立边。
    const auto& edge_vertices = mesh.vtk_edge_cells_;
    for (size_t i = 0; i + 1 < edge_vertices.size(); i += 2) {
        const Index p0 = edge_vertices[i];
        const Index p1 = edge_vertices[i + 1];
        if (p0 >= 0 && p1 >= 0 && p0 != p1)
            resolve_edge(p0, p1);
    }

    // 面边按无序端点归并，并记录使用该边的面单元。
    const auto& face_vertices = mesh.vtk_face_cells_;
    const auto& offsets = mesh.vtk_face_cells_offset_;
    const Index face_count = offsets.empty() ? 0 : static_cast<Index>(offsets.size() - 1);
    for (Index face = 0; face < face_count; ++face) {
        const Index begin = offsets[static_cast<size_t>(face)];
        const Index end = offsets[static_cast<size_t>(face + 1)];
        if (begin < 0 || end > static_cast<Index>(face_vertices.size()) || end - begin < 2)
            continue;

        for (Index i = begin; i < end; ++i) {
            const Index p0 = face_vertices[static_cast<size_t>(i)];
            const Index p1 = face_vertices[static_cast<size_t>(i + 1 < end ? i + 1 : begin)];
            if (p0 < 0 || p1 < 0 || p0 == p1)
                continue;
            auto& adjacent_faces = resolve_edge(p0, p1).adjacent_faces;
            if (adjacent_faces.empty() || adjacent_faces.back() != face)
                adjacent_faces.push_back(face);
        }
    }
    return edges;
}

//! @brief 计算多边形前三个非共线顶点定义的单位法向
std::array<double, 3> faceNormal(const MeshDataVtk& mesh, Index face)
{
    const Index begin = mesh.vtk_face_cells_offset_[static_cast<size_t>(face)];
    const Index end = mesh.vtk_face_cells_offset_[static_cast<size_t>(face + 1)];
    if (end - begin < 3)
        return {};

    const Index p0_id = mesh.vtk_face_cells_[static_cast<size_t>(begin)];
    if (p0_id < 0 || p0_id >= static_cast<Index>(mesh.vertex_positions_.size()))
        return {};
    const auto& p0 = mesh.vertex_positions_[static_cast<size_t>(p0_id)];
    for (Index i = begin + 1; i + 1 < end; ++i) {
        const Index p1_id = mesh.vtk_face_cells_[static_cast<size_t>(i)];
        const Index p2_id = mesh.vtk_face_cells_[static_cast<size_t>(i + 1)];
        if (p1_id < 0 || p2_id < 0
            || p1_id >= static_cast<Index>(mesh.vertex_positions_.size())
            || p2_id >= static_cast<Index>(mesh.vertex_positions_.size())) {
            continue;
        }
        const auto& p1 = mesh.vertex_positions_[static_cast<size_t>(p1_id)];
        const auto& p2 = mesh.vertex_positions_[static_cast<size_t>(p2_id)];
        const std::array<double, 3> a { p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2] };
        const std::array<double, 3> b { p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2] };
        std::array<double, 3> normal {
            a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0]
        };
        const double length = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
        if (length > 0.0) {
            for (double& value : normal)
                value /= length;
            return normal;
        }
    }
    return {};
}

//! @brief 计算两个相邻面的内二面角：180°减去单位法向夹角，范围为 [0, 180]
double dihedralAngle(const std::array<double, 3>& first, const std::array<double, 3>& second)
{
    const double first_length = std::sqrt(first[0] * first[0] + first[1] * first[1] + first[2] * first[2]);
    const double second_length = std::sqrt(second[0] * second[0] + second[1] * second[1] + second[2] * second[2]);
    if (first_length == 0.0 || second_length == 0.0)
        return -1.0;

    const double dot = std::clamp(first[0] * second[0] + first[1] * second[1] + first[2] * second[2], -1.0, 1.0);
    constexpr double radians_to_degrees = 180.0 / 3.14159265358979323846;
    const double normal_angle = std::acos(dot) * radians_to_degrees;
    return 180.0 - normal_angle;
}

//! @brief 判断一个点的一环面是否由一条连通扇或闭合环组成
bool isNonManifoldVertex(const std::vector<MeshEdgeTopology>& incident_edges)
{
    std::unordered_map<Index, std::vector<Index>> face_graph;
    Index boundary_edge_count = 0;
    for (const MeshEdgeTopology& edge : incident_edges) {
        // 非流形边及其端点归入边类别，非流形点仅表示独立的一环面扇异常。
        if (edge.adjacent_faces.size() > 2)
            return false;
        if (edge.adjacent_faces.size() == 1)
            ++boundary_edge_count;
        if (edge.adjacent_faces.size() == 2) {
            face_graph[edge.adjacent_faces[0]].push_back(edge.adjacent_faces[1]);
            face_graph[edge.adjacent_faces[1]].push_back(edge.adjacent_faces[0]);
        } else if (edge.adjacent_faces.size() == 1) {
            face_graph.try_emplace(edge.adjacent_faces[0]);
        }
    }
    if (face_graph.empty())
        return false;

    std::unordered_set<Index> visited;
    std::vector<Index> pending { face_graph.begin()->first };
    while (!pending.empty()) {
        const Index face = pending.back();
        pending.pop_back();
        if (!visited.insert(face).second)
            continue;
        for (Index neighbor : face_graph[face])
            pending.push_back(neighbor);
    }

    // 流形内部点没有边界边；流形边界点恰有两条边界边。一环断裂或分叉均为非流形。
    return visited.size() != face_graph.size() || (boundary_edge_count != 0 && boundary_edge_count != 2);
}
}

MeshTopologyDiagnosticResult MeshTopologyDiagnostics::analyze(const MeshDataVtk& mesh)
{
    MeshTopologyDiagnosticResult result;
    const Index point_count = static_cast<Index>(mesh.vertex_positions_.size());
    std::vector<bool> point_used(static_cast<size_t>(point_count), false);

    const Index face_count = mesh.vtk_face_cells_offset_.empty()
        ? 0
        : static_cast<Index>(mesh.vtk_face_cells_offset_.size() - 1);
    std::vector<std::array<double, 3>> normals(static_cast<size_t>(face_count));
    std::vector<bool> boundary_faces(static_cast<size_t>(face_count), false);
    for (Index face = 0; face < face_count; ++face) {
        const Index begin = mesh.vtk_face_cells_offset_[static_cast<size_t>(face)];
        const Index end = mesh.vtk_face_cells_offset_[static_cast<size_t>(face + 1)];
        if (begin < 0 || end > static_cast<Index>(mesh.vtk_face_cells_.size()) || end - begin < 2)
            continue;
        normals[static_cast<size_t>(face)] = faceNormal(mesh, face);
    }

    const std::vector<MeshEdgeTopology> edges = buildEdgeTopologies(mesh);
    std::vector<std::vector<MeshEdgeTopology>> vertex_edges(static_cast<size_t>(point_count));
    for (const MeshEdgeTopology& edge : edges) {
        if (edge.endpoints[0] < 0 || edge.endpoints[1] < 0
            || edge.endpoints[0] >= point_count || edge.endpoints[1] >= point_count
            || edge.endpoints[0] == edge.endpoints[1]) {
            continue;
        }
        point_used[static_cast<size_t>(edge.endpoints[0])] = true;
        point_used[static_cast<size_t>(edge.endpoints[1])] = true;
        TopologyDiagnosticEdge diagnostic_edge { edge.endpoints, -1.0 };
        if (edge.adjacent_faces.empty()) {
            result.isolated_edges.push_back(diagnostic_edge);
        } else if (edge.adjacent_faces.size() == 1) {
            result.boundary_edges.push_back(diagnostic_edge);
            boundary_faces[static_cast<size_t>(edge.adjacent_faces.front())] = true;
        } else if (edge.adjacent_faces.size() == 2) {
            diagnostic_edge.dihedral_angle_degrees = dihedralAngle(
                normals[static_cast<size_t>(edge.adjacent_faces[0])], normals[static_cast<size_t>(edge.adjacent_faces[1])]);
            result.manifold_edges.push_back(diagnostic_edge);
        } else {
            result.non_manifold_edges.push_back(diagnostic_edge);
        }
        vertex_edges[static_cast<size_t>(edge.endpoints[0])].push_back(edge);
        vertex_edges[static_cast<size_t>(edge.endpoints[1])].push_back(edge);
    }

    for (Index face = 0; face < face_count; ++face) {
        if (boundary_faces[static_cast<size_t>(face)])
            result.boundary_faces.push_back(face);
    }
    for (Index point = 0; point < point_count; ++point) {
        if (!point_used[static_cast<size_t>(point)]) {
            result.isolated_vertices.push_back(point);
        } else if (isNonManifoldVertex(vertex_edges[static_cast<size_t>(point)])) {
            result.non_manifold_vertices.push_back(point);
        }
    }
    return result;
}
