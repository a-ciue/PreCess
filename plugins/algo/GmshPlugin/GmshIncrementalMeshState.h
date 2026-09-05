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

// 保存本次 CAD 面划分得到的临时结果，用于合并进 MeshData 并生成通用面映射。
struct SingleFaceMeshResult {
    std::vector<std::array<double, 3>> vertices;
    std::vector<std::size_t> face_vertices;
    std::vector<std::size_t> face_vertices_offset;
    // 写入 MeshData 的组件内局部点 id 序列（与 face_vertices 一一对应）。
    std::vector<Index> global_face_vertices;
    bool success { false };
};

// 保存本次操作使用的 Gmsh 共享边临时缓存；面拓扑统一由 GeometryMeshMap 持有。
struct GmshIncrementalMeshState {
    // 已划分 CAD 边的节点缓存，key 使用 geometry 的全局 CAD 边 ID。
    std::map<GeomEdgeId, MeshedEdgeData> meshedEdgesCache;
    // 由 GeometryMeshMap 重建的临时引用计数，用于快速判断边缓存能否删除。
    std::map<GeomEdgeId, int> meshedEdgeRefCounts;
};
