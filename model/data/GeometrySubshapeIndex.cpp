#include "GeometrySubshapeIndex.h"
#include "GeometryRegistry.h"

#include <spdlog/spdlog.h>

#include <TopExp.hxx>
#include <TopoDS_Shape.hxx>
#include <algorithm>
#include <stdexcept>
#include <string>

namespace {
/**
 * @brief 为某类子形状分配/回收全局 id
 *
 * local_to_global 预填充（快照恢复路径）时按原值 reclaim——gid 向量是身份数据，
 * 发号只增不复用，原值必然空闲；为空时全新分配。
 *
 * @throw std::runtime_error 预填充向量长度与子形状数不匹配（局部下标约定被破坏）
 */
template <typename GeomId>
void assignGlobalIds(GeometryRegistry& reg, const TopTools_IndexedMapOfShape& map,
    std::vector<GeomId>& local_to_global,
    GeomId (GeometryRegistry::*alloc)(const TopoDS_Shape&),
    void (GeometryRegistry::*reclaim)(GeomId, const TopoDS_Shape&),
    GeomId invalid_id, const char* type_name)
{
    const int n = map.Extent();
    if (!local_to_global.empty()) {
        if (static_cast<int>(local_to_global.size()) != n + 1)
            throw std::runtime_error(std::string("GeometrySubshapeIndex::build: saved ") + type_name
                + " gid count mismatches subshape count");
        for (int local = 1; local <= n; ++local) {
            const GeomId gid = local_to_global[local];
            if (gid != invalid_id)
                (reg.*reclaim)(gid, map.FindKey(local));
        }
        return;
    }
    local_to_global.assign(n + 1, invalid_id);
    for (int local = 1; local <= n; ++local)
        local_to_global[local] = (reg.*alloc)(map.FindKey(local));
}
}

int GeometrySubshapeIndex::typeIndex(TopAbs_ShapeEnum type)
{
    // 我们只支持 TopAbs_COMPOUND..TopAbs_VERTEX
    if (type < TopAbs_COMPOUND || type > TopAbs_VERTEX)
        return -1;
    return static_cast<int>(type); // OCCT enum 这段正好是连续的 0..7
}

void GeometrySubshapeIndex::build(const TopoDS_Shape& root, GeometryRegistry& reg)
{
    built = false;

    // 1) 构建各 OCC 类型的局部索引
    for (int ti = 0; ti < kTypeCount; ++ti) {
        type_maps[ti].Clear();

        const TopAbs_ShapeEnum type = static_cast<TopAbs_ShapeEnum>(ti);
        TopExp::MapShapes(root, type, type_maps[ti]);
    }

    {
        const int nVertices = type_maps[typeIndex(TopAbs_VERTEX)].Extent();
        const int nEdges    = type_maps[typeIndex(TopAbs_EDGE)].Extent();
        const int nWires    = type_maps[typeIndex(TopAbs_WIRE)].Extent();
        const int nFaces    = type_maps[typeIndex(TopAbs_FACE)].Extent();
        const int nShells   = type_maps[typeIndex(TopAbs_SHELL)].Extent();
        const int nSolids   = type_maps[typeIndex(TopAbs_SOLID)].Extent();
        const int nCompounds = type_maps[typeIndex(TopAbs_COMPOUND)].Extent();
        const int nCompSolids = type_maps[typeIndex(TopAbs_COMPSOLID)].Extent();

        spdlog::info("[GeometryIndex] sub-shape summary:"
            " Vertex={}, Edge={}, Wire={}, Face={}, Shell={}, Solid={}, CompSolid={}, Compound={}",
            nVertices, nEdges, nWires, nFaces, nShells, nSolids, nCompSolids, nCompounds);
    }

    // 2) 业务主类型的 localTypeId -> 全局 id（快照恢复时按原值 reclaim，见 assignGlobalIds）
    assignGlobalIds(reg, type_maps[typeIndex(TopAbs_VERTEX)], vertex_local_to_global,
        &GeometryRegistry::allocVertex, &GeometryRegistry::reclaimVertex,
        kInvalidGeomVertexId, "vertex");
    assignGlobalIds(reg, type_maps[typeIndex(TopAbs_EDGE)], edge_local_to_global,
        &GeometryRegistry::allocEdge, &GeometryRegistry::reclaimEdge,
        kInvalidGeomEdgeId, "edge");
    assignGlobalIds(reg, type_maps[typeIndex(TopAbs_FACE)], face_local_to_global,
        &GeometryRegistry::allocFace, &GeometryRegistry::reclaimFace,
        kInvalidGeomFaceId, "face");
    assignGlobalIds(reg, type_maps[typeIndex(TopAbs_SOLID)], solid_local_to_global,
        &GeometryRegistry::allocSolid, &GeometryRegistry::reclaimSolid,
        kInvalidGeomSolidId, "solid");

    built = true;
}

void GeometrySubshapeIndex::release(GeometryRegistry& reg)
{
    if (!built)
        return;

    for (GeomVertexId id : vertex_local_to_global)
        reg.eraseVertex(id);
    for (GeomEdgeId id : edge_local_to_global)
        reg.eraseEdge(id);
    for (GeomFaceId id : face_local_to_global)
        reg.eraseFace(id);
    for (GeomSolidId id : solid_local_to_global)
        reg.eraseSolid(id);

    vertex_local_to_global.clear();
    edge_local_to_global.clear();
    face_local_to_global.clear();
    solid_local_to_global.clear();

    for (auto& m : type_maps)
        m.Clear();

    built = false;
}

GeomVertexId GeometrySubshapeIndex::vertexGlobalId(int localTypeId) const
{
    if (localTypeId <= 0 || localTypeId >= (int)vertex_local_to_global.size())
        return kInvalidGeomVertexId;
    return vertex_local_to_global[localTypeId];
}

GeomEdgeId GeometrySubshapeIndex::edgeGlobalId(int localTypeId) const
{
    if (localTypeId <= 0 || localTypeId >= (int)edge_local_to_global.size())
        return kInvalidGeomEdgeId;
    return edge_local_to_global[localTypeId];
}

GeomFaceId GeometrySubshapeIndex::faceGlobalId(int localTypeId) const
{
    if (localTypeId <= 0 || localTypeId >= (int)face_local_to_global.size())
        return kInvalidGeomFaceId;
    return face_local_to_global[localTypeId];
}

GeomSolidId GeometrySubshapeIndex::solidGlobalId(int localTypeId) const
{
    if (localTypeId <= 0 || localTypeId >= (int)solid_local_to_global.size())
        return kInvalidGeomSolidId;
    return solid_local_to_global[localTypeId];
}