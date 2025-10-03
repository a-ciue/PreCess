#ifndef MAKE_MESH_DATA_VTK_H
#define MAKE_MESH_DATA_VTK_H

#include "MeshActor.h"

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

#endif
