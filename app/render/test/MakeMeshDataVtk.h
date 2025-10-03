#ifndef MAKE_MESH_DATA_VTK_H
#define MAKE_MESH_DATA_VTK_H

#include "MeshActor.h"
#include <string_view>

// 生成内置演示用数据
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

    std::vector<Index>& vtk_edge_cells_
);

// 从外部 vtk/vtu/vtp 文件导入，并尽可能填充 MeshDataVtk
// 当前实现：
//  - 读取 UnstructuredGrid 或 PolyData
//  - 直接按 cell 维度分类：3D->solid, 2D->face, 1D->edge
//  - polyhedral 相关数组留空
//  - face block 生成单一 block，包含全部面；若需要更复杂 block 划分，可在此函数外再修改 block_datas
MeshDataVtk MakeMeshDataVtkFromFile(
    std::string_view file_path,
    std::vector<std::array<double, 3>>& vtk_points_,

    std::vector<unsigned char>& vtk_solid_cell_types_,
    std::vector<Index>& vtk_solid_cells_,
    std::vector<Index>& vtk_solid_cells_offset_,
    std::vector<Index>& vtk_solid_faces_,
    std::vector<Index>& vtk_solid_faces_offset_,
    std::vector<Index>& vtk_solid_face_locations_,
    std::vector<Index>& vtk_solid_face_locations_offset_,

    std::vector<Index>& vtk_face_cells_,
    std::vector<Index>& vtk_face_cells_offset_,

    std::vector<Index>& vtk_edge_cells_
);

#endif
