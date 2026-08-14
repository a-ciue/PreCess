#include "MeshTopologyDiagnostics.h"

#include "MeshData.h"
#include "MeshAdjacency.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace {
//! @brief 计算多边形前三个非共线顶点定义的单位法向
std::array<double, 3> faceNormal(const MeshData& mesh, Index face)
{
    const Index begin = mesh.face_vertices_offset_[face];
    const Index end = mesh.face_vertices_offset_[face + 1];
    if (end - begin < 3)
        return {};

    const Index p0_id = mesh.face_vertices_[begin];
    if (p0_id < 0 || p0_id >= static_cast<Index>(mesh.vertex_positions_.size()))
        return {};
    const auto& p0 = mesh.vertex_positions_[p0_id];
    for (Index i = begin + 1; i + 1 < end; ++i) {
        const Index p1_id = mesh.face_vertices_[i];
        const Index p2_id = mesh.face_vertices_[i + 1];
        if (p1_id < 0 || p2_id < 0
            || p1_id >= static_cast<Index>(mesh.vertex_positions_.size())
            || p2_id >= static_cast<Index>(mesh.vertex_positions_.size())) {
            continue;
        }
        const auto& p1 = mesh.vertex_positions_[p1_id];
        const auto& p2 = mesh.vertex_positions_[p2_id];
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

//! @brief 计算两个相邻面单位法向的无符号夹角，范围为 [0, 180]
double dihedralAngle(const std::array<double, 3>& first, const std::array<double, 3>& second)
{
    const double first_length = std::sqrt(first[0] * first[0] + first[1] * first[1] + first[2] * first[2]);
    const double second_length = std::sqrt(second[0] * second[0] + second[1] * second[1] + second[2] * second[2]);
    if (first_length == 0.0 || second_length == 0.0)
        return -1.0;

    const double dot = std::clamp(first[0] * second[0] + first[1] * second[1] + first[2] * second[2], -1.0, 1.0);
    constexpr double radians_to_degrees = 180.0 / 3.14159265358979323846;
    return std::acos(dot) * radians_to_degrees;
}

//! @brief 判断一个点的一环面是否由一条连通扇或闭合环组成
bool isNonManifoldVertex(const std::vector<MeshEdgeTopology>& incident_edges)
{
    std::unordered_map<Index, std::vector<Index>> face_graph;
    Index boundary_edge_count = 0;
    for (const MeshEdgeTopology& edge : incident_edges) {
        if (edge.adjacent_faces.size() > 2)
            return true;
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

MeshTopologyDiagnosticResult MeshTopologyDiagnostics::analyze(MeshAdjacency& adjacency, const MeshData& mesh)
{
    MeshTopologyDiagnosticResult result;
    const Index point_count = static_cast<Index>(mesh.vertex_positions_.size());
    std::vector<bool> point_used(static_cast<size_t>(point_count), false);

    const Index face_count = mesh.face_vertices_offset_.empty()
        ? 0
        : static_cast<Index>(mesh.face_vertices_offset_.size() - 1);
    std::vector<std::array<double, 3>> normals(static_cast<size_t>(face_count));
    std::vector<bool> boundary_faces(static_cast<size_t>(face_count), false);
    for (Index face = 0; face < face_count; ++face) {
        const Index begin = mesh.face_vertices_offset_[face];
        const Index end = mesh.face_vertices_offset_[face + 1];
        if (begin < 0 || end > static_cast<Index>(mesh.face_vertices_.size()) || end - begin < 2)
            continue;
        normals[static_cast<size_t>(face)] = faceNormal(mesh, face);
    }

    const std::vector<MeshEdgeTopology> edges = adjacency.edgeTopologies(mesh);
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
