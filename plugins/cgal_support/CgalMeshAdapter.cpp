/**
 * @file CgalMeshAdapter.cpp
 * @brief MeshData 与 CGAL Surface_mesh 的双向转换实现
 */

#include "CgalMeshAdapter.h"

#include <stdexcept>
#include <string>
#include <unordered_map>

CgalMesh toSurfaceMesh(const MeshData& mesh, const std::vector<std::array<double, 3>>& global_points)
{
    CgalMesh sm;

    // 与 InpModelHandler 一致：仅当 vertex_positions_ 已清空且 local_to_global_ 非空时按全局约定解析
    const bool is_global = mesh.vertex_positions_.empty() && !mesh.local_to_global_.empty();

    // 顶点坐标：全局约定下 vertex_positions_ 已被 ModelLayer 清空，需从全局点数组取；
    // 按序加入，Surface_mesh 顶点索引与 MeshData 局部索引保持一致
    if (is_global) {
        for (const Index gid : mesh.local_to_global_) {
            if (gid < 0 || static_cast<size_t>(gid) >= global_points.size())
                throw std::runtime_error("全局点 id 越界: " + std::to_string(gid));
            const auto& p = global_points[gid];
            sm.add_vertex(CgalPoint3(p[0], p[1], p[2]));
        }
    } else {
        for (const auto& p : mesh.vertex_positions_)
            sm.add_vertex(CgalPoint3(p[0], p[1], p[2]));
    }

    // face_vertices_ 索引约定：全局约定下存全局点 id，经逆映射换回局部索引
    std::unordered_map<Index, Index> global_to_local;
    if (is_global) {
        for (size_t i = 0; i < mesh.local_to_global_.size(); ++i)
            global_to_local[mesh.local_to_global_[i]] = static_cast<Index>(i);
    }

    // 面仅接受三角面
    for (size_t f = 0; f + 1 < mesh.face_vertices_offset_.size(); ++f) {
        const Index begin = mesh.face_vertices_offset_[f];
        const Index end = mesh.face_vertices_offset_[f + 1];
        if (end - begin != 3)
            throw std::runtime_error("网格含非三角面，当前仅支持全三角网格");

        std::vector<CgalMesh::Vertex_index> face;
        face.reserve(3);
        for (Index i = begin; i < end; ++i) {
            const Index id = mesh.face_vertices_[i];
            if (is_global) {
                const auto it = global_to_local.find(id);
                if (it == global_to_local.end())
                    throw std::runtime_error("面引用的顶点不在组件内，全局 id: " + std::to_string(id));
                face.push_back(CgalMesh::Vertex_index(static_cast<size_t>(it->second)));
            } else {
                if (id < 0 || static_cast<size_t>(id) >= mesh.vertex_positions_.size())
                    throw std::runtime_error("面顶点索引越界: " + std::to_string(id));
                face.push_back(CgalMesh::Vertex_index(static_cast<size_t>(id)));
            }
        }
        sm.add_face(face);
    }
    return sm;
}

void fromSurfaceMesh(const CgalMesh& sm, MeshData& out)
{
    // 清空旧数据（含边/体/patch/属性），避免复用 out 时残留；clear() 会清空所有 offset 数组，
    // 按 MeshData 约定补回 {0} 哨兵表示无对应单元（输出为局部索引约定，local_to_global_ 保持空）
    out.clear();
    out.solid_vertices_offset_ = { 0 };
    out.solid_faces_vertices_offset_ = { 0 };
    out.solid_faces_offset_ = { 0 };

    // 源网格经删除操作后索引可能不连续，按遍历序建立到紧凑索引的重排表
    std::vector<Index> remap(sm.num_vertices(), -1);
    out.vertex_positions_.reserve(sm.number_of_vertices());
    for (const auto v : sm.vertices()) {
        remap[static_cast<size_t>(v)] = static_cast<Index>(out.vertex_positions_.size());
        const auto& p = sm.point(v);
        out.vertex_positions_.push_back({ p.x(), p.y(), p.z() });
    }
    out.vertex_count_ = static_cast<Index>(out.vertex_positions_.size());

    out.face_vertices_offset_.reserve(sm.number_of_faces() + 1);
    out.face_vertices_offset_.push_back(0);
    for (const auto f : sm.faces()) {
        const CgalMesh::Halfedge_index h0 = sm.halfedge(f);
        CgalMesh::Halfedge_index h = h0;
        do {
            out.face_vertices_.push_back(remap[static_cast<size_t>(sm.target(h))]);
            h = sm.next(h);
        } while (h != h0);
        out.face_vertices_offset_.push_back(static_cast<Index>(out.face_vertices_.size()));
    }
}
