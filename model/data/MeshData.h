#pragma once
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <array>

#include "Core.h"

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
    std::vector<Index> faces; //> 存放对应面的索引

    // 构造函数
    Patch() = delete;
    Patch(Index id, Index block) : id_(id), blockID(block) {}
};

/**
 * @brief 表示网格中的一个 Block（块）
 *
 * Block 由多个 Patch 组成，具有唯一 ID
 */
struct Block {
    std::unordered_set<Index> patchIDs;
    Index id;
};

struct MeshData {
    using PatchMap = std::unordered_map<Index, std::unique_ptr<Patch>>;
    using BlockMap = std::unordered_map<Index, std::unique_ptr<Block>>;

    /**
     * @brief 多边形面网格点id索引
     * 
        0, 1, 2,
        2, 3, 0,
        3, 2, 6,
        6, 7, 3,
        4, 0, 3,
        3, 7, 4,
        7, 6, 5,
        5, 4, 7,
        4, 5, 1,
        1, 0, 4,
        1, 5, 6,
        6, 2, 1
     */
    std::vector<Index> face_vertices;

    /**
     * @brief 存储每个面的顶点偏移量的索引数组。

        { 0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33 }
     */
    std::vector<Index> face_vertex_offsets;

    /**
     * @brief 存储顶点的三维坐标位置。

        { 0.000000, 2.000000, 2.000000 }, { 0.000000, 0.000000, 2.000000 },
        { 2.000000, 0.000000, 2.000000 }, { 2.000000, 2.000000, 2.000000 },
        { 0.000000, 2.000000, 0.000000 }, { 0.000000, 0.000000, 0.000000 },
        { 2.000000, 0.000000, 0.000000 }, { 2.000000, 2.000000, 0.000000 }
     */
    std::vector<std::array<double, 3>> vertex_positions;

    PatchMap patches_;
    BlockMap blocks_;
};
