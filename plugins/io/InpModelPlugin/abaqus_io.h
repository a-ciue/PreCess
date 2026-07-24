#pragma once

#include <string>
#include <unordered_map>
#include <vector>

struct CellBlock {
    std::string type;
    std::vector<std::vector<int>> data; // zero-based node indices
    // original Abaqus TYPE (if present) to allow round-trip writing
    std::string abaqus_type;
};

struct Mesh_meshIO {
    std::vector<std::vector<double>> points; // [npoints][dim]
    std::vector<CellBlock> cells;

    std::unordered_map<std::string, std::vector<int>> point_sets;
    // cell_sets maps name -> list of arrays, one per cell block
    std::unordered_map<std::string, std::vector<std::vector<int>>> cell_sets;
};

// Read Abaqus .inp file and return Mesh
Mesh_meshIO read_abaqus(const std::string& filename);

// Write Mesh to Abaqus .inp
void write_abaqus(const std::string& filename, const Mesh_meshIO& mesh, const std::string& float_fmt = "%.16e", bool translate_cell_names = true);
