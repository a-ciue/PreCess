#ifndef MAKE_MESH_DATA_VTK_H
#define MAKE_MESH_DATA_VTK_H

#include "MeshActor.h"
#include "MeshData.h"
#include <string_view>
#include <vector>

// 生成内置演示用数据
// point_gids 输出全局点 id（iota 恒等填充），由调用方持有并须活得与返回的 MeshDataVtk 一样久
MeshDataVtk MakeMeshDataVtk(MeshData& data, std::vector<Index>& point_gids);

// 从外部 vtk/vtu/vtp 文件导入，并尽可能填充 MeshDataVtk
// 当前实现：
//  - 读取 UnstructuredGrid 或 PolyData
//  - 直接按 cell 维度分类：3D->solid, 2D->face, 1D->edge
//  - polyhedral 相关数组留空
//  - face block 生成单一 block，包含全部面；若需要更复杂 block 划分，可在此函数外再修改 block_datas
MeshDataVtk MakeMeshDataVtkFromFile(
    std::string_view file_path,
    MeshData& data,
    std::vector<Index>& point_gids
);

#endif
