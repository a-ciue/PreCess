#pragma once
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <array>

#include "Core.h"
#include <string>

#include <optional>

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
     * @brief 顶点的三维坐标（组件自持）。
     *
     * MeshData 为自包含数据：坐标常驻本数组，face_vertices_ / edge_vertices_ /
     * solid_vertices_ / solid_faces_vertices_ 等连通性数组均存储本数组的局部下标
     * （局部点 id），可整体快照/恢复（撤销重做依赖此性质）。
     *
     *  { 0.000000, 2.000000, 2.000000 }, { 0.000000, 0.000000, 2.000000 },
     *  { 2.000000, 0.000000, 2.000000 }, { 2.000000, 2.000000, 2.000000 },
     *  { 0.000000, 2.000000, 0.000000 }, { 0.000000, 0.000000, 0.000000 },
     *  { 2.000000, 0.000000, 0.000000 }, { 2.000000, 2.000000, 0.000000 }
     *
     * @note 局部点 id 在组件生命周期内保持稳定：只增不改号、不重排、不压实
     *       （MeshAdjacency 持久边身份层以端点对为键，依赖此不变式）；
     *       全局点 id 由 ComponentData::point_global_ids_ 经 MeshIDMap 分配。
     */
    std::vector<std::array<double, 3>> vertex_positions_;
    Index vertex_count_ { 0 }; //> 恒等于 vertex_positions_.size()

    /**
     * @brief 面的点索引，存储构成网格面单元的顶点索引。可以表示独立于体单元之外单独定义的面单元。
     * 
     *  {0, 1, 2,
     *  2, 3, 0,
     *  3, 2, 6,
     *  6, 7, 3,
     *  4, 0, 3,
     *  3, 7, 4,
     *  7, 6, 5,
     *  5, 4, 7,
     *  4, 5, 1,
     *  1, 0, 4,
     *  1, 5, 6,
     *  6, 2, 1}
     */
    std::vector<Index> face_vertices_;
    /**
     * @brief 面的点索引偏移，存储每个面单元的顶点偏移量数组。可以表示独立于体单元之外单独定义的面。
     * 
     *  { 0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33 } 表示了 offsets size - 1 = 11个面单元
     */
    std::vector<Index> face_vertices_offset_;

    /**
     * @brief 边的点索引，边单元的顶点索引数组，可以表示独立于体、面的边单元。每两个顶点索引表示一条边。
     */
    std::vector<Index> edge_vertices_;

    /**
     * @brief 体单元的类型，对应每个体单元的 VTK 类型编号。
     *
     * 例如，六面体 VTK_HEXAHEDRON = 12, 三棱柱 VTK_WEDGE = 13, 任意多面体 VTK_POLYHEDRON = 42 等。参见 https://vtk.org/doc/nightly/html/vtkCellType_8h.html
     * 注意：对于可存储任意形状的多面体 VTK_POLYHEDRON，需要同时使用 solid_faces_vertices_、solid_faces_vertices_offset_、solid_faces_ 和 solid_faces_offset_ 来定义其面的拓扑结构。
     */
    std::vector<unsigned char> solid_types_;
    /**
     * @brief 体的点索引，体单元的顶点索引数组，每个体单元的顶点索引连续存储。
     *
     * 例如，对于一个六面体（立方体）VTK_HEXAHEDRON，会有8个顶点索引按VTK定义的顺序依次存储，后面接着存放其他体单元的顶点索引。
     */
    std::vector<Index> solid_vertices_;
    /**
     * @brief 体的点索引偏移，存储每个体单元的顶点偏移量。
     *
     * 例如：{ 0, 8, 14 } 表示两个体单元，第0个体单元由 solid_vertices_ 中的第 [0,8) 个顶点构成，第1个体单元由 solid_vertices_ 中的第 [8,14) 个顶点构成。
     * 注意，offset数组虽然可以留空，但起码要有{0}表示没有cell。
     */
    std::vector<Index> solid_vertices_offset_ { 0 };
    /**
     * @brief 体的面的点索引，存储构成多面体 vtkPolyhedron 的面的顶点（角点）索引，顺序存储。
     *
     * 例如，有很多的面彼此组合成了多面体，每个面的顶点索引顺序存储在该数组中，表示了每个面是由这些顶点顺次连接来的。
     * 注意：vtk 有明确定义体单元类型的不必要存储，只需按照vtk定义的顺序将顶点按序存入 solid_vertices_ 和 solid_vertices_offset_ 即可。此处单纯针对多面体 vtkPolyhedron 的情形，即更为复杂的情形，所以数组可以为空。
     */
    std::vector<Index> solid_faces_vertices_;
    /**
     * @brief 体的面的点索引偏移，存储构成多面体的面的顶点索引偏移量，同 solid_faces_vertices_ 一起定义了多面体的各个面。
     *
     * 例如：{ 0, 4, 7, 10 } 表示 solid_faces_vertices_ 中第 [0,4) 点构成第0个面，第[4,7)面构成第1个面，第[7,10)面构成第2个面。
     * 注意，offset数组虽然可以留空，但起码要有{0}表示没有face。
     */
    std::vector<Index> solid_faces_vertices_offset_ { 0 };
    /**
     * @brief 体的面索引，存储构成每个多面体的是哪些面。
     *
     * 例如，一个多面体由多个面构成，构成每个多面体的面id顺次存储在该数组中。
     */
    std::vector<Index> solid_faces_;
    /**
     * @brief 体的面索引的偏移，存储多面体单元的面索引偏移量，同 solid_faces_ 一起定义了多面体的各个面。
     *
     * 例如：{ 0, 6, 12 } 表示 solid_faces_ 中第 [0,6) 面构成第0个多面体，第[6,12)面构成第1个多面体。
     * 注意：如果没有多面体（即所有体单元均为简单类型），该数组最少也要包含一个元素 {0}，表示没有多面体。
     */
    std::vector<Index> solid_faces_offset_ { 0 };

    PatchMap patches_;
    BlockMap blocks_;

    /*来存储增加的表示点面边体的属性*/
 
    std::map<std::string, std::vector<double>> vertex_attributes_;
    std::map<std::string, std::vector<double>> face_attributes_;
    std::map<std::string, std::vector<double>> edge_attributes_;
    std::map<std::string, std::vector<double>> solid_attributes_;
 
    /**
     * @brief 清除所有数据，即使offset数组起码也要保留一个元素0
     */
    void clear();
    /**
     * @brief 初始化 MeshData 结构，确保 offset 数组至少包含一个初始元素 0。
     */
    void init();
    /**
     * @brief 根据给定的面ID，查找其所属的Patch ID
     * @param face_id 面的索引
     * @return 如果找到对应的Patch ID，则返回该ID，否则返回std::nullopt
     */
    std::optional<Index> face_patch_id(int face_id);
    /**
     * @brief 根据给定的Patch ID，查找其所属的Block ID
     * @param patch_id Patch的索引
     * @return 如果找到对应的Block ID，则返回该ID，否则返回std::nullopt
     */
    std::optional<Index> patch_block_id(int patch_id);
};
