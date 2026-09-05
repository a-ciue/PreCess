#pragma once
#include "Core.h"
#include <unordered_map>

#include <TopoDS_Shape.hxx>

class GeometryRegistry {
public:
    GeomVertexId allocVertex(const TopoDS_Shape& s);
    GeomEdgeId allocEdge(const TopoDS_Shape& s);
    GeomFaceId allocFace(const TopoDS_Shape& s);
    GeomSolidId allocSolid(const TopoDS_Shape& s);

    const TopoDS_Shape* getVertex(GeomVertexId id) const;
    const TopoDS_Shape* getEdge(GeomEdgeId id) const;
    const TopoDS_Shape* getFace(GeomFaceId id) const;
    const TopoDS_Shape* getSolid(GeomSolidId id) const;

    void eraseVertex(GeomVertexId id);
    void eraseEdge(GeomEdgeId id);
    void eraseFace(GeomFaceId id);
    void eraseSolid(GeomSolidId id);

    /**
     * @brief 按原值回收几何 gid（快照恢复用）
     *
     * 发号器只增不减、erase 留空洞不复用，故原 gid 在释放后必然空闲，无需 free-list。
     * @throw std::runtime_error id 越界（>= 发号水位）或仍被占用
     */
    void reclaimVertex(GeomVertexId id, const TopoDS_Shape& s);
    void reclaimEdge(GeomEdgeId id, const TopoDS_Shape& s);
    void reclaimFace(GeomFaceId id, const TopoDS_Shape& s);
    void reclaimSolid(GeomSolidId id, const TopoDS_Shape& s);

private:
    GeomVertexId next_vertex_id_ { 0 };
    GeomEdgeId next_edge_id_ { 0 };
    GeomFaceId next_face_id_ { 0 };
    GeomSolidId next_solid_id_ { 0 };

    std::unordered_map<GeomVertexId, TopoDS_Shape> vertex_id_to_shape_;
    std::unordered_map<GeomEdgeId, TopoDS_Shape> edge_id_to_shape_;
    std::unordered_map<GeomFaceId, TopoDS_Shape> face_id_to_shape_;
    std::unordered_map<GeomSolidId, TopoDS_Shape> solid_id_to_shape_;
};