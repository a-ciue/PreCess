#include "GeometryRegistry.h"
#include <TopoDS_Shape.hxx>
#include <spdlog/spdlog.h>
#include <TopAbs_ShapeEnum.hxx>

GeomVertexId GeometryRegistry::allocVertex(const TopoDS_Shape& s)
{
    GeomVertexId id = next_vertex_id_++;
    vertex_id_to_shape_.emplace(id, s);
    return id;
}

GeomEdgeId GeometryRegistry::allocEdge(const TopoDS_Shape& s)
{
    GeomEdgeId id = next_edge_id_++;
    edge_id_to_shape_.emplace(id, s);
    return id;
}

GeomFaceId GeometryRegistry::allocFace(const TopoDS_Shape& s)
{
    GeomFaceId id = next_face_id_++;
    face_id_to_shape_.emplace(id, s);
    return id;
}

GeomSolidId GeometryRegistry::allocSolid(const TopoDS_Shape& s)
{
    GeomSolidId id = next_solid_id_++;
    solid_id_to_shape_.emplace(id, s);
    return id;
}

const TopoDS_Shape* GeometryRegistry::getVertex(GeomVertexId id) const
{
    auto it = vertex_id_to_shape_.find(id);
    return it == vertex_id_to_shape_.end() ? nullptr : &it->second;
}

const TopoDS_Shape* GeometryRegistry::getEdge(GeomEdgeId id) const
{
    auto it = edge_id_to_shape_.find(id);
    return it == edge_id_to_shape_.end() ? nullptr : &it->second;
}

const TopoDS_Shape* GeometryRegistry::getFace(GeomFaceId id) const 
{
    auto it = face_id_to_shape_.find(id);
    return it == face_id_to_shape_.end() ? nullptr : &it->second;
}

const TopoDS_Shape* GeometryRegistry::getSolid(GeomSolidId id) const
{
    auto it = solid_id_to_shape_.find(id);
    return it == solid_id_to_shape_.end() ? nullptr : &it->second;
}

void GeometryRegistry::eraseVertex(GeomVertexId id)
{
    vertex_id_to_shape_.erase(id);
}
void GeometryRegistry::eraseEdge(GeomEdgeId id)
{
    edge_id_to_shape_.erase(id);
}
void GeometryRegistry::eraseFace(GeomFaceId id)
{
    face_id_to_shape_.erase(id);
}
void GeometryRegistry::eraseSolid(GeomSolidId id)
{
    solid_id_to_shape_.erase(id);
}