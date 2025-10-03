#include "MakeMeshDataVtk.h"
#include <vtkCellType.h>

MeshDataVtk MakeMeshDataVtk(
    std::vector<std::array<double, 3>>& vtk_points_,

    std::vector<unsigned char>& vtk_solid_cell_types_,
    std::vector<Index>& vtk_solid_cells_,
    std::vector<Index>& vtk_solid_cells_offset_,
    std::vector<Index>& vtk_solid_faces_,
    std::vector<Index>& vtk_solid_faces_offset_,
    std::vector<Index>& vtk_solid_face_locations_,
    std::vector<Index>& vtk_solid_face_locations_offset_,

    std::vector<Index>& vtk_face_cells_, //> 表示面顶点索引的数组
    std::vector<Index>& vtk_face_cells_offset_,

    std::vector<Index>& vtk_edge_cells_)
{
    // 更复杂的测试模型：
    // Solid: 1个六面体 (Hexahedron) + 1个楔体 (Wedge)
    // Independent Faces: 1个四边形 + 1个三角形
    // Independent Edges: 立方体12条边 + 楔体9条边 + 2条独立线段

    // 顶点说明：
    // 0-7   : 立方体
    // 8-11  : 独立四边形
    // 12-13 : 第一条独立线段
    // 14-19 : 楔体 (wedge) 六个点 (底 14,15,16  顶 17,18,19)
    // 20-22 : 独立三角形
    // 使用到的最大索引为 22, 共 23 个点
    vtk_points_ = {
        // 立方体 (0-7)
        { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 1.0, 1.0, 0.0 }, { 0.0, 1.0, 0.0 },
        { 0.0, 0.0, 1.0 }, { 1.0, 0.0, 1.0 }, { 1.0, 1.0, 1.0 }, { 0.0, 1.0, 1.0 },
        // 独立四边形 (8-11)
        { 1.5, 0.0, 0.0 }, { 2.5, 0.0, 0.0 }, { 2.5, 1.0, 0.0 }, { 1.5, 1.0, 0.0 },
        // 独立线段1 (12-13)
        { 0.0, 1.5, 0.0 }, { 1.0, 1.5, 0.0 },
        // 楔体 Wedge (14-19) 位置放在 x ~ 3 附近
        { 3.0, 0.0, 0.0 }, { 4.0, 0.0, 0.0 }, { 3.5, 1.0, 0.0 },
        { 3.0, 0.0, 1.0 }, { 4.0, 0.0, 1.0 }, { 3.5, 1.0, 1.0 },
        // 独立三角形 (20-22)
        { 2.0, 1.5, 0.0 }, { 2.5, 1.5, 0.0 }, { 2.25, 2.0, 0.0 }
    };

    // 面（包含：六面体6个四边形 + 楔体5个面(2三角+3四边形) + 1个独立四边形 + 1个独立三角形）
    // 共 6 + 5 + 1 + 1 = 13 个面
    // 使用可变长度offset表示不同面顶点数量
    vtk_face_cells_ = {
        // 立方体6个四边形 (每个4点)
        0, 3, 2, 1, // 0
        4, 5, 6, 7, // 1
        0, 1, 5, 4, // 2
        2, 3, 7, 6, // 3
        0, 4, 7, 3, // 4
        1, 2, 6, 5, // 5
        // 楔体 2个三角形
        14, 15, 16, // 6  (三角)
        17, 19, 18, // 7  (三角, 注意次序)
        // 楔体 3个四边形
        14, 17, 18, 15, // 8
        15, 18, 19, 16, // 9
        16, 19, 17, 14, // 10
        // 独立四边形
        8, 9, 10, 11, // 11
        // 独立三角形
        20, 21, 22 // 12
    };
    // offsets: 每个面在 face_cells 中的起始位置 (最后一个为总长度)
    vtk_face_cells_offset_ = {
        0, // 面0
        4, // 面1
        8, // 面2
        12, // 面3
        16, // 面4
        20, // 面5
        24, // 面6 (三角)
        27, // 面7 (三角)
        30, // 面8
        34, // 面9
        38, // 面10
        42, // 面11
        46, // 面12
        49 // 结束
    };

    // 边：立方体12 + 楔体9 + 独立线段2 = 23 条边
    vtk_edge_cells_ = {
        // 立方体 12 条边 (0-7)
        0, 1, 1, 2, 2, 3, 3, 0,
        4, 5, 5, 6, 6, 7, 7, 4,
        0, 4, 1, 5, 2, 6, 3, 7,
        // 独立线段1
        12, 13,
        // 楔体 9 条边 (底3 + 顶3 + 竖向3)
        14, 15, 15, 16, 16, 14,
        17, 18, 18, 19, 19, 17,
        14, 17, 15, 18, 16, 19,
        // 独立线段2 (三角的一条边)
        20, 21
    };

    // 体：1个六面体 + 1个楔体
    vtk_solid_cell_types_ = { VTK_HEXAHEDRON, VTK_WEDGE };
    // 连续节点索引，使用 offset 描述不同 cell 的节点数量
    vtk_solid_cells_ = {
        // Hex (8点)
        0, 1, 2, 3, 4, 5, 6, 7,
        // Wedge (6点, VTK 约定顺序：底三角0,1,2  顶三角3,4,5)
        14, 15, 16, 17, 18, 19
    };
    vtk_solid_cells_offset_ = { 0, 8, 14 }; // 2 个cell

    // Polyhedral 数据：标准VTK_HEXAHEDRON / VTK_WEDGE 不需要 polyhedral faces 描述
    //std::vector<Index> vtk_solid_faces_; // 留空
    //std::vector<Index> vtk_solid_faces_offset_; // 留空
    //std::vector<Index> vtk_solid_face_locations_; // 留空
    //std::vector<Index> vtk_solid_face_locations_offset_; // 留空

    // Block:
    // Block1 -> 立方体 6个面 (0-5)
    // Block2 -> 楔体    5个面 (6-10)
    // Block3 -> 独立面  2个面 (11,12)
    auto block_datas = std::make_shared<BlockDatas>();
    {
        BlockData b1;
        b1.id = 1;
        b1.faces_ = { 0, 1, 2, 3, 4, 5 };
        block_datas->block_datas.push_back(b1);
        BlockData b2;
        b2.id = 2;
        b2.faces_ = { 6, 7, 8, 9, 10 };
        block_datas->block_datas.push_back(b2);
        BlockData b3;
        b3.id = 3;
        b3.faces_ = { 11, 12 };
        block_datas->block_datas.push_back(b3);
    }

    // 组装 MeshDataVtk
    MeshDataVtk test_mesh_data = {
        vtk_points_,
        vtk_solid_cell_types_,
        vtk_solid_cells_,
        vtk_solid_cells_offset_,
        vtk_solid_faces_,
        vtk_solid_faces_offset_,
        vtk_solid_face_locations_,
        vtk_solid_face_locations_offset_,
        vtk_face_cells_,
        vtk_face_cells_offset_,
        vtk_edge_cells_,
        block_datas
    };

    return test_mesh_data;
}