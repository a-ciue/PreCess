#pragma once

#include <memory>
#include <vector>

class TopoDS_Shape;
class TopoDS_Face;
class TopoDS_Edge;

// 为增量网格划分缓存 OCC 拓扑索引，避免每次划分单个面时重复遍历整棵 Shape。
class IncrementalMeshContext {
public:
    explicit IncrementalMeshContext(const TopoDS_Shape& shape);
    ~IncrementalMeshContext();

    IncrementalMeshContext(const IncrementalMeshContext&) = delete;
    IncrementalMeshContext& operator=(const IncrementalMeshContext&) = delete;

    // 返回当前 Shape 中注册到全局 OCC 边索引的边数量。
    int globalEdgeCount() const;
    // 返回指定面包含的全局 OCC 边 ID，供 Gmsh 边界复用逻辑使用。
    std::vector<int> getFaceEdgeIds(const TopoDS_Face& face) const;
    // 根据全局 OCC 边 ID 取回原始 TopoDS_Edge。
    TopoDS_Edge getEdgeByGlobalId(int globalId) const;

    // 返回当前 Shape 中可独立划分的面数量。
    std::size_t faceCount() const;
    // 根据 0 起始面索引取回 TopoDS_Face。
    TopoDS_Face getFaceByIndex(std::size_t index) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
