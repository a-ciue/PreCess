#include "GeometrySubshapeIndex.h"
#include "GeometryRegistry.h"

#include <TopExp.hxx>
#include <TopoDS_Shape.hxx>
#include <algorithm>

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

    // 2) Vertex: localTypeId -> GeomVertexId
    {
        const int ti = typeIndex(TopAbs_VERTEX);
        vertex_local_to_global.assign(type_maps[ti].Extent() + 1, kInvalidGeomVertexId);

        for (int localTypeId = 1; localTypeId <= type_maps[ti].Extent(); ++localTypeId) {
            const TopoDS_Shape& sub = type_maps[ti].FindKey(localTypeId);
            vertex_local_to_global[localTypeId] = reg.allocVertex(sub);
        }
    }

    // 3) Edge: localTypeId -> GeomEdgeId
    {
        const int ti = typeIndex(TopAbs_EDGE);
        edge_local_to_global.assign(type_maps[ti].Extent() + 1, kInvalidGeomEdgeId);

        for (int localTypeId = 1; localTypeId <= type_maps[ti].Extent(); ++localTypeId) {
            const TopoDS_Shape& sub = type_maps[ti].FindKey(localTypeId);
            edge_local_to_global[localTypeId] = reg.allocEdge(sub);
        }
    }

    // 4) Face: localTypeId -> GeomFaceId
    {
        const int ti = typeIndex(TopAbs_FACE);
        face_local_to_global.assign(type_maps[ti].Extent() + 1, kInvalidGeomFaceId);

        for (int localTypeId = 1; localTypeId <= type_maps[ti].Extent(); ++localTypeId) {
            const TopoDS_Shape& sub = type_maps[ti].FindKey(localTypeId);
            face_local_to_global[localTypeId] = reg.allocFace(sub);
        }
    }

    // 5) Solid: localTypeId -> GeomSolidId
    {
        const int ti = typeIndex(TopAbs_SOLID);
        solid_local_to_global.assign(type_maps[ti].Extent() + 1, kInvalidGeomSolidId);

        for (int localTypeId = 1; localTypeId <= type_maps[ti].Extent(); ++localTypeId) {
            const TopoDS_Shape& sub = type_maps[ti].FindKey(localTypeId);
            solid_local_to_global[localTypeId] = reg.allocSolid(sub);
        }
    }

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