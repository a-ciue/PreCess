#pragma once
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <array>
#include <qstring.h>

#include "ToolMesh.h"
#include "Core.h"

namespace MeshLib {
    template <typename V, typename E, typename F, typename H>
    class CToolMesh;
    class CToolVertex;
    class CToolEdge;
    class CToolFace;
    class CToolHalfEdge;
    typedef CToolMesh<CToolVertex, CToolEdge, CToolFace, CToolHalfEdge> CTMesh;
}

/**
 * @brief 表示网格中的一个 Patch
 *
 * Patch 由多个三角形面组成，并包含其在全局网格中的 ID 信息
 * 同时Patch为模型的自身属性，不随相关操作而更改
 */
struct Patch {
    // patch id
    Index id_ { -1 };
    Index blockID { -1 };

    // 构造函数
    Patch() = delete;
    Patch(Index id, Index block) : id_(id), blockID(block) {}
};

/**
 * @brief 表示网格中的一个 Block（块）
 *
 * Block 由多个 Patch 组成，具有唯一 ID，并归属于某个 Group。
 */
struct Block {
    std::unordered_set<Index> patchIDs;
    Index id;
};

struct MeshData {
    using PatchMap = std::unordered_map<Index, std::unique_ptr<Patch>>;
    using BlockMap = std::unordered_map<Index, std::unique_ptr<struct Block>>;

    // 三角形的局部id索引
    std::vector <std::array<Index, 3>> faceTriangles_;
    // 坐标
    std::vector <std::array<double, 3>> vertexPoints_;

    PatchMap patches_;
    BlockMap blocks_;

    QString model_name_;
    // ctor 声明
    explicit MeshData(std::unique_ptr<MeshLib::CTMesh> mesh);
    // 优化 update_patches 的实现，减少网格遍历次数
    void update_patches(const std::vector<Index>& patch_ids, bool new_patch = true);
    void update_patches(const std::unordered_set<Index>& patch_ids, bool new_patch = true);
};
