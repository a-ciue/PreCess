#pragma once
#include <memory>
#include <optional>
#include <array>
#include <map>
#include <vector>
#include "GeometrySubshapeIndex.h"

class TopoDS_Shape;
struct GeometryDataVtk;
class IncrementalMeshContext;

// 保存一条已生成网格的 CAD 边，供相邻面复用边界节点。
struct MeshedEdgeData {
    std::vector<double> coords; 
    std::vector<double> paramCoords; 

    std::size_t nodeCount() const { return coords.size() / 3; }
    bool empty() const { return coords.empty(); }
};

// 保存单个 CAD 面的网格结果，面删除和重划分时用它重建整体 MeshData。
struct SingleFaceMeshResult {
    std::vector<std::array<double, 3>> vertices;
    std::vector<std::size_t> face_vertices;
    std::vector<std::size_t> face_vertices_offset;
    bool success { false };
};

// GeometryData 上的 Gmsh 增量网格状态，生命周期跟随当前 CAD component。
struct GmshIncrementalMeshState {
    GmshIncrementalMeshState();
    ~GmshIncrementalMeshState();

    std::unique_ptr<IncrementalMeshContext> meshContext;
    std::map<int, MeshedEdgeData> meshedEdgesCache;
    std::map<std::size_t, SingleFaceMeshResult> meshedFacesCache;
    std::map<int, int> meshedEdgeRefCounts;
    std::vector<Index> local_to_global_point_ids;

    // 清空当前 CAD component 的增量网格缓存，不影响 GeometryData::rootShape。
    void clear();
};

// 以后需要控制点 / 曲率等，可继续添加字段
struct GeometryData {
    GeometryData();
	~GeometryData();
    std::unique_ptr<TopoDS_Shape> rootShape;      // 读取 STEP/IGES 后的拓扑根
    GeometrySubshapeIndex index;
    void ensureIndexBuilt(GeometryRegistry& reg);

    //std::vector<TopoDS_Edge>     edges;      // 可选：拆分得到的边
    //std::vector<TopoDS_Vertex>   controlPts; // 可选：用于渲染／算法

    std::optional<GeometryDataVtk> getGeometryData();
};
