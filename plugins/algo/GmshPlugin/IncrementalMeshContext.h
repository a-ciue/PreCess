#pragma once

#include "Core.h"

#include <vector>

class TopoDS_Face;
class TopoDS_Edge;
struct GeometryData;
struct GeometrySubshapeIndex;
class GeometryRegistry;

// 将 GeometryRegistry 的 CAD 子形状索引适配给 Gmsh 增量网格流程使用。
class IncrementalMeshContext {
public:
    IncrementalMeshContext(GeometryData& geometry, GeometryRegistry& registry);
    ~IncrementalMeshContext();

    IncrementalMeshContext(const IncrementalMeshContext&) = delete;
    IncrementalMeshContext& operator=(const IncrementalMeshContext&) = delete;

    // 返回当前 CAD component 中注册到 GeometryRegistry 的边数量。
    int globalEdgeCount() const;
    // 返回指定面包含的全局 CAD 边 ID，供 Gmsh 边界复用逻辑使用。
    std::vector<GeomEdgeId> getFaceEdgeIds(const TopoDS_Face& face) const;
    // 根据全局 CAD 边 ID 取回原始 TopoDS_Edge。
    TopoDS_Edge getEdgeByGlobalId(GeomEdgeId globalId) const;

    // 返回当前 Shape 中可独立划分的面数量。
    std::size_t faceCount() const;
    // 根据 0 起始面索引取回 TopoDS_Face。
    TopoDS_Face getFaceByIndex(std::size_t index) const;

private:
    GeometrySubshapeIndex* cad_index_ {};
    GeometryRegistry* registry_ {};
};
