#pragma once

#include "MeshData.h"
#include "abaqus_io.h"
#include <iostream>
#include <map>
#include <vector>

// VTK cell type definitions
namespace VTK {
const unsigned char VERTEX = 1;
const unsigned char POLY_VERTEX = 2;
const unsigned char LINE = 3;
const unsigned char POLY_LINE = 4;
const unsigned char TRIANGLE = 5;
const unsigned char TRIANGLE_STRIP = 6;
const unsigned char POLYGON = 7;
const unsigned char PIXEL = 8;
const unsigned char QUAD = 9;
const unsigned char TETRA = 10;
const unsigned char VOXEL = 11;
const unsigned char HEXAHEDRON = 12;
const unsigned char WEDGE = 13;
const unsigned char PYRAMID = 14;
const unsigned char HEXAGONAL_PRISM = 15;
const unsigned char PENTAGONAL_PRISM = 16;
}

// Map from meshio cell type to VTK cell type
static std::map<std::string, unsigned char> meshio_to_vtk_type = {
    { "line", VTK::LINE },
    { "line3", VTK::POLY_LINE },
    { "triangle", VTK::TRIANGLE },
    { "triangle6", VTK::POLYGON },
    { "quad", VTK::QUAD },
    { "quad8", VTK::POLYGON },
    { "quad9", VTK::POLYGON },
    { "tetra", VTK::TETRA },
    { "tetra4", VTK::TETRA },
    { "tetra10", VTK::TETRA },
    { "hexahedron", VTK::HEXAHEDRON },
    { "hexahedron20", VTK::HEXAHEDRON },
    { "wedge", VTK::WEDGE },
    { "wedge15", VTK::WEDGE }
};

/**
 * @brief Convert abaqus_io Mesh structure to MeshData structure
 * @param abaqus_mesh Input mesh from abaqus_io
 * @param mesh_data Output mesh in MeshData format
 */
void convert_abaqus_to_meshdata(const Mesh_meshIO& abaqus_mesh, MeshData& mesh_data);

/**
 * @brief Convert internal MeshData to intermediate Mesh_meshIO (用于写出 .inp)
 * @param mesh_data 输入的 MeshData
 * @param abaqus_mesh 输出中间表示（用于 write_abaqus）
 */
void convert_meshdata_to_abaqus(const MeshData& mesh_data, Mesh_meshIO& abaqus_mesh);

/**
 * @brief 根据 vtk type 找到 meshio type 字符串
 */
std::string meshio_type_from_vtk(unsigned char vtk_type);