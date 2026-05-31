#pragma once

#include "Core.h"

#include <array>
#include <map>
#include <memory>
#include <vector>

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

// 保存一个 component 的 Gmsh 增量网格状态，由 Gmsh 插件管理生命周期。
struct GmshIncrementalMeshState {
    GmshIncrementalMeshState();
    ~GmshIncrementalMeshState();

    std::unique_ptr<IncrementalMeshContext> meshContext;
    std::map<int, MeshedEdgeData> meshedEdgesCache;
    std::map<std::size_t, SingleFaceMeshResult> meshedFacesCache;
    std::map<int, int> meshedEdgeRefCounts;
    std::vector<Index> local_to_global_point_ids;

    // 清空当前 component 的增量网格缓存。
    void clear();
};
