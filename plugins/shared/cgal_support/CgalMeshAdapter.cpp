/**
 * @file CgalMeshAdapter.cpp
 * @brief MeshData 与 CGAL Surface_mesh 的双向转换实现
 */

#include "CgalMeshAdapter.h"

#include <CGAL/number_utils.h>

#include <stdexcept>
#include <string>

namespace {

//! 依据内核 K 构建网格的共享实现（K 提供 Point_3 类型）
template <class K>
using SmOf = CGAL::Surface_mesh<typename K::Point_3>;

//! MeshData -> Surface_mesh<K::Point_3>：全三角面校验 + 顶点索引越界校验
template <class K>
SmOf<K> buildFromMeshData(const MeshData& mesh)
{
    SmOf<K> sm;
    for (const auto& p : mesh.vertex_positions_)
        sm.add_vertex(typename K::Point_3(p[0], p[1], p[2]));

    for (size_t f = 0; f + 1 < mesh.face_vertices_offset_.size(); ++f) {
        const Index begin = mesh.face_vertices_offset_[f];
        const Index end = mesh.face_vertices_offset_[f + 1];
        if (end - begin != 3)
            throw std::runtime_error("网格含非三角面，当前仅支持全三角网格");

        std::vector<typename SmOf<K>::Vertex_index> face;
        face.reserve(3);
        for (Index i = begin; i < end; ++i) {
            const Index id = mesh.face_vertices_[i];
            if (id < 0 || static_cast<size_t>(id) >= mesh.vertex_positions_.size())
                throw std::runtime_error("面顶点索引越界: " + std::to_string(id));
            face.push_back(typename SmOf<K>::Vertex_index(static_cast<size_t>(id)));
        }
        sm.add_face(face);
    }
    return sm;
}

//! Surface_mesh<K::Point_3> -> MeshData：顶点按遍历序重排为紧凑索引
template <class K>
void storeIntoMeshData(const SmOf<K>& sm, MeshData& out)
{
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
        out.vertex_positions_.push_back({ CGAL::to_double(p.x()), CGAL::to_double(p.y()), CGAL::to_double(p.z()) });
    }
    out.vertex_count_ = static_cast<Index>(out.vertex_positions_.size());

    out.face_vertices_offset_.reserve(sm.number_of_faces() + 1);
    out.face_vertices_offset_.push_back(0);
    for (const auto f : sm.faces()) {
        const typename SmOf<K>::Halfedge_index h0 = sm.halfedge(f);
        typename SmOf<K>::Halfedge_index h = h0;
        do {
            out.face_vertices_.push_back(remap[static_cast<size_t>(sm.target(h))]);
            h = sm.next(h);
        } while (h != h0);
        out.face_vertices_offset_.push_back(static_cast<Index>(out.face_vertices_.size()));
    }
}

} // namespace

CgalMesh toSurfaceMesh(const MeshData& mesh)
{
    return buildFromMeshData<CgalKernel>(mesh);
}

CgalExactMesh toExactSurfaceMesh(const MeshData& mesh)
{
    return buildFromMeshData<CgalExactKernel>(mesh);
}

void fromSurfaceMesh(const CgalMesh& sm, MeshData& out)
{
    storeIntoMeshData<CgalKernel>(sm, out);
}

void fromSurfaceMesh(const CgalExactMesh& sm, MeshData& out)
{
    storeIntoMeshData<CgalExactKernel>(sm, out);
}
