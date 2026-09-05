#pragma once
#include "Core.h"
#include <array>
#include <vector>

#include <TopAbs_ShapeEnum.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS_Shape.hxx>

class GeometryRegistry;

struct GeometrySubshapeIndex {
    bool built { false };

    // 按 OCC 类型的局部索引
    static constexpr int kTypeCount = 8; // TopAbs_COMPOUND..TopAbs_VERTEX
    std::array<NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>, kTypeCount> type_maps;

    // 按业务主类型分开的全局 id 映射（身份数据：快照保留、恢复时按原值 reclaim，见 build 注释）
    std::vector<GeomVertexId> vertex_local_to_global;
    std::vector<GeomEdgeId> edge_local_to_global;
    std::vector<GeomFaceId> face_local_to_global;
    std::vector<GeomSolidId> solid_local_to_global;

    void build(const TopoDS_Shape& root, GeometryRegistry& reg);
    void release(GeometryRegistry& reg);

    GeomVertexId vertexGlobalId(int localTypeId) const;
    GeomEdgeId edgeGlobalId(int localTypeId) const;
    GeomFaceId faceGlobalId(int localTypeId) const;
    GeomSolidId solidGlobalId(int localTypeId) const;

    static int typeIndex(TopAbs_ShapeEnum type);
};