#pragma once

#include "Core.h"

#include <array>
#include <map>
#include <vector>

// 保存一条已生成网格的 CAD 边，供相邻面复用边界节点。
struct MeshedEdgeData {
    std::vector<double> coords;
    std::vector<double> paramCoords;
};

// 保存单个 CAD 面的网格结果，面删除和重划分时用它重建整体 MeshData。
struct SingleFaceMeshResult {
    std::vector<std::array<double, 3>> vertices;
    std::vector<std::size_t> face_vertices;
    std::vector<std::size_t> face_vertices_offset;
    // 写入 MeshData 后对应的全局点 ID，用于只删除本次 Gmsh 生成的单元。
    std::vector<Index> global_face_vertices;
    bool success { false };
};

// 保存一个 component 的 Gmsh 增量网格状态，由 Gmsh 插件管理生命周期。
struct GmshIncrementalMeshState {
    // 已划分 CAD 边的节点缓存，key 使用 geometry 的全局 CAD 边 ID。
    std::map<GeomEdgeId, MeshedEdgeData> meshedEdgesCache;
    // 已划分 CAD 面的网格结果，key 使用 geometry 的全局 CAD 面 ID。
    std::map<GeomFaceId, SingleFaceMeshResult> meshedFacesCache;
    // 已划分 CAD 边被多少个面复用，用于删除面网格时判断是否清理边缓存。
    std::map<GeomEdgeId, int> meshedEdgeRefCounts;
};
