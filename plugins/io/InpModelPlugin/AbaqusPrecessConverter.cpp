#include "AbaqusPrecessConverter.h"

#include <algorithm>
#include <unordered_map>
#include <spdlog/spdlog.h>

const std::map<std::string, unsigned char> meshio_to_vtk_type = {
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

void convert_abaqus_to_meshdata(const Mesh_meshIO& abaqus_mesh, MeshData& mesh_data)
{
    mesh_data.init();

    // 1. Convert vertex positions
    // abaqus_mesh.points: vector<vector<double>> [npoints][dim]
    // mesh_data.vertex_positions_: vector<array<double, 3>>
    for (const auto& pt : abaqus_mesh.points) {
        std::array<double, 3> vertex;
        if (pt.size() >= 3) {
            vertex[0] = pt[0];
            vertex[1] = pt[1];
            vertex[2] = pt[2];
        } else if (pt.size() == 2) {
            vertex[0] = pt[0];
            vertex[1] = pt[1];
            vertex[2] = 0.0;
        } else {
            vertex[0] = pt[0];
            vertex[1] = 0.0;
            vertex[2] = 0.0;
        }
        mesh_data.vertex_positions_.push_back(vertex);
    }

    mesh_data.vertex_count_ = static_cast<Index>(mesh_data.vertex_positions_.size());

    // 2. Convert cells (elements)
    // Determine if cells are 2D (faces) or 3D (solids) based on type
    for (const auto& cell_block : abaqus_mesh.cells) {
        const std::string& cell_type = cell_block.type;
        auto vtk_it = meshio_to_vtk_type.find(cell_type);
        if (vtk_it == meshio_to_vtk_type.end()) {
            spdlog::warn("AbaqusPrecessConverter: Unknown cell type '{}', skipping.", cell_type);
            continue;
        }
        unsigned char vtk_type = vtk_it->second;

        // 2D elements -> face data
        if (cell_type == "triangle" || cell_type == "triangle6" || cell_type == "quad" || cell_type == "quad8" || cell_type == "quad9") {

            Index offset;
            if (mesh_data.face_vertices_offset_.empty()) {
                offset = 0;
            } else {
                offset = mesh_data.face_vertices_offset_.back();
            }
            for (const auto& elem : cell_block.data) {
                for (Index node_id : elem) {
                    mesh_data.face_vertices_.push_back(node_id);
                }
                offset += elem.size();
                mesh_data.face_vertices_offset_.push_back(offset);
            }
        }
        // 3D elements -> solid data
        else if (cell_type == "tetra" || cell_type == "tetra4" || cell_type == "tetra10" || cell_type == "hexahedron" || cell_type == "hexahedron20" || cell_type == "wedge" || cell_type == "wedge15") {

            Index offset;
            if (mesh_data.solid_vertices_offset_.empty()) {
                offset = 0;
            } else {
                offset = mesh_data.solid_vertices_offset_.back();
            }
            for (const auto& elem : cell_block.data) {
                mesh_data.solid_types_.push_back(vtk_type);
                for (Index node_id : elem) {
                    mesh_data.solid_vertices_.push_back(node_id);
                }
                offset += elem.size();
                mesh_data.solid_vertices_offset_.push_back(offset);
            }
        }
        // 1D elements -> edge data
        else if (cell_type == "line" || cell_type == "line3") {
            for (const auto& elem : cell_block.data) {
                for (Index node_id : elem) {
                    mesh_data.edge_vertices_.push_back(node_id);
                }
            }
        }
    }
    spdlog::debug("AbaqusPrecessConverter: conversion completed - Vertices={}, Faces={}, Edges={}, Solids={}",
                  mesh_data.vertex_positions_.size(),
                  mesh_data.face_vertices_offset_.size() - 1,
                  mesh_data.edge_vertices_.size() / 2,
                  mesh_data.solid_vertices_offset_.size() - 1);
}

std::string meshio_type_from_vtk(unsigned char vtk_type)
{
    for (const auto& kv : meshio_to_vtk_type) {
        if (kv.second == vtk_type)
            return kv.first;
    }
    // fallback
    if (vtk_type == VTK::TRIANGLE)
        return "triangle";
    if (vtk_type == VTK::QUAD)
        return "quad";
    if (vtk_type == VTK::TETRA)
        return "tetra";
    if (vtk_type == VTK::HEXAHEDRON)
        return "hexahedron";
    if (vtk_type == VTK::WEDGE)
        return "wedge";
    if (vtk_type == VTK::LINE)
        return "line";
    return "unknown";
}

void convert_meshdata_to_abaqus(const MeshData& mesh_data, Mesh_meshIO& abaqus_mesh)
{
    abaqus_mesh.points.clear();
    abaqus_mesh.cells.clear();
    abaqus_mesh.point_sets.clear();
    abaqus_mesh.cell_sets.clear();

    bool use_vertex_positions = !mesh_data.vertex_positions_.empty();
    abaqus_mesh.points.reserve(static_cast<size_t>(mesh_data.vertex_count_));
    
    if (use_vertex_positions) {
        for (const auto& p : mesh_data.vertex_positions_) {
            abaqus_mesh.points.push_back({ p[0], p[1], p[2] });
        }
    }

    // solids (体单元)：根据 solid_vertices_offset_ 切分 solid_vertices_
    if (!mesh_data.solid_vertices_offset_.empty()) {
        const auto& offsets = mesh_data.solid_vertices_offset_;
        size_t nsolids = offsets.size() > 0 ? offsets.size() - 1 : 0;
        // group cells by type to reduce number of CellBlock entries
        // 使用 map<type, CellBlockIndex>
        std::unordered_map<std::string, size_t> block_index_by_type;

        for (size_t i = 0; i < nsolids; ++i) {
            Index start = offsets[i];
            Index end = offsets[i + 1];
            std::vector<int> nodes;
            nodes.reserve(end - start);
            for (Index k = start; k < end; ++k) {
                // MeshData 中存储的是 Index（通常为 int），abaqus_io 使用 zero-based indices
                nodes.push_back(static_cast<int>(mesh_data.solid_vertices_[k]));
            }

            unsigned char vtk_type = 0;
            if (i < mesh_data.solid_types_.size())
                vtk_type = mesh_data.solid_types_[i];
            std::string meshio_type = meshio_type_from_vtk(vtk_type);

            size_t block_idx;
            auto it = block_index_by_type.find(meshio_type);
            if (it == block_index_by_type.end()) {
                CellBlock cb;
                cb.type = meshio_type;
                cb.data.clear();
                cb.abaqus_type.clear();
                abaqus_mesh.cells.push_back(std::move(cb));
                block_idx = abaqus_mesh.cells.size() - 1;
                block_index_by_type.emplace(meshio_type, block_idx);
            } else {
                block_idx = it->second;
            }
            abaqus_mesh.cells[block_idx].data.push_back(std::move(nodes));
        }
    }

    // faces (作为独立的面单元)：使用 face_vertices_offset_
    if (!mesh_data.face_vertices_offset_.empty()) {
        const auto& foffsets = mesh_data.face_vertices_offset_;
        size_t nfaces = foffsets.size() > 0 ? foffsets.size() - 1 : 0;
        if (nfaces > 0) {
            // 简单地以顶点数决定类型，三角面 -> triangle，否则 polygon（或 quad）
            // 把所有面放在同一 cell block per type
            std::unordered_map<std::string, size_t> face_block_idx;
            for (size_t i = 0; i < nfaces; ++i) {
                Index s = foffsets[i];
                Index e = foffsets[i + 1];
                size_t cnt = e - s;
                std::string type;
                if (cnt == 3)
                    type = "triangle";
                else if (cnt == 4)
                    type = "quad";
                else
                    // type = "polygon";
                    type = "CPE6";
                // 会发生把CPE6写成polygon

                std::vector<int> nodes;
                nodes.reserve(cnt);
                for (Index k = s; k < e; ++k)
                    nodes.push_back(static_cast<int>(mesh_data.face_vertices_[k]));

                size_t idx;
                auto it = face_block_idx.find(type);
                if (it == face_block_idx.end()) {
                    CellBlock cb;
                    cb.type = type;
                    cb.data.clear();
                    cb.abaqus_type.clear();
                    abaqus_mesh.cells.push_back(std::move(cb));
                    idx = abaqus_mesh.cells.size() - 1;
                    face_block_idx.emplace(type, idx);
                } else {
                    idx = it->second;
                }
                abaqus_mesh.cells[idx].data.push_back(std::move(nodes));
            }
        }
    }

    // edges (边单元)：每两点为一条边
    if (!mesh_data.edge_vertices_.empty()) {
        size_t nedges = mesh_data.edge_vertices_.size() / 2;
        if (nedges > 0) {
            std::string type = "line";
            // find or create block
            size_t idx = abaqus_mesh.cells.size();
            bool found = false;
            for (size_t i = 0; i < abaqus_mesh.cells.size(); ++i) {
                if (abaqus_mesh.cells[i].type == type) {
                    idx = i;
                    found = true;
                    break;
                }
            }
            if (!found) {
                CellBlock cb;
                cb.type = type;
                cb.data.clear();
                cb.abaqus_type.clear();
                abaqus_mesh.cells.push_back(std::move(cb));
                idx = abaqus_mesh.cells.size() - 1;
            }
            for (size_t i = 0; i < nedges; ++i) {
                std::vector<int> nodes;
                nodes.push_back(static_cast<int>(mesh_data.edge_vertices_[2 * i]));
                nodes.push_back(static_cast<int>(mesh_data.edge_vertices_[2 * i + 1]));
                abaqus_mesh.cells[idx].data.push_back(std::move(nodes));
            }
        }
    }

    // NOTE: 不在此处处理 vertex_attributes_ / face_attributes_ 等属性的写入，
    // 如果需要可以将其存入 abaqus_mesh 的 point_sets / cell_sets 或扩展 CellBlock::abaqus_type 等字段。
}