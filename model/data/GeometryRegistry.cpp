#include "GeometryRegistry.h"
#include <TopoDS_Shape.hxx>
#include <spdlog/spdlog.h>
#include <TopAbs_ShapeEnum.hxx>

#include <stdexcept>
#include <string>

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

namespace {
//! @brief reclaim 公共校验与插入：id 须在发号水位内且当前空闲（erase 后的空洞）
template <typename GeomId>
void reclaimInto(std::unordered_map<GeomId, TopoDS_Shape>& map, GeomId watermark,
    GeomId id, const TopoDS_Shape& s, const char* type_name)
{
    if (id < 0 || id >= watermark)
        throw std::runtime_error(std::string("GeometryRegistry::reclaim") + type_name
            + ": id " + std::to_string(id) + " out of watermark");
    if (!map.emplace(id, s).second)
        throw std::runtime_error(std::string("GeometryRegistry::reclaim") + type_name
            + ": id " + std::to_string(id) + " still occupied");
}
}

void GeometryRegistry::reclaimVertex(GeomVertexId id, const TopoDS_Shape& s)
{
    reclaimInto(vertex_id_to_shape_, next_vertex_id_, id, s, "Vertex");
}

void GeometryRegistry::reclaimEdge(GeomEdgeId id, const TopoDS_Shape& s)
{
    reclaimInto(edge_id_to_shape_, next_edge_id_, id, s, "Edge");
}

void GeometryRegistry::reclaimFace(GeomFaceId id, const TopoDS_Shape& s)
{
    reclaimInto(face_id_to_shape_, next_face_id_, id, s, "Face");
}

void GeometryRegistry::reclaimSolid(GeomSolidId id, const TopoDS_Shape& s)
{
    reclaimInto(solid_id_to_shape_, next_solid_id_, id, s, "Solid");
}