#include "OBJMeshIO.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <spdlog/spdlog.h>
#include <numeric>
#include <fstream>

std::unique_ptr<MeshData> ObjMeshIO::loadFromFile(const std::filesystem::path& filename)
{
    tinyobj::ObjReaderConfig reader_config;
    reader_config.mtl_search_path = "."; // Path to material files
    reader_config.triangulate = false; // Do not triangulate faces

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(filename.string(), reader_config)) {
        if (!reader.Error().empty()) {
            spdlog::error("TinyObjReader: " + reader.Error());
        }
        return nullptr;
    }

    if (!reader.Warning().empty()) {
        spdlog::warn("TinyObjReader: " + reader.Warning());
    }

    auto& attrib = reader.GetAttrib();
    auto& shapes = reader.GetShapes();
    auto& materials = reader.GetMaterials();

    auto result = std::make_unique<MeshData>();
    MeshData& mesh_data = *result;
    for (size_t v = 0; v < attrib.vertices.size() / 3; v++) {
        mesh_data.vertex_positions.push_back({ attrib.vertices[3 * v + 0], attrib.vertices[3 * v + 1], attrib.vertices[3 * v + 2] });
    }
    // Loop over shapes(groups)
    mesh_data.face_vertex_offsets.push_back(0);
    for (auto& shape: shapes) {
        Index new_face = static_cast<Index>(mesh_data.face_vertex_offsets.size() - 1);

        // 面的点索引序列 mesh_data.face_vertices
        std::transform(shape.mesh.indices.begin(), shape.mesh.indices.end(),
            std::back_inserter(mesh_data.face_vertices),
            [](const tinyobj::index_t& idx) { return static_cast<Index>(idx.vertex_index); });


        // 追加面顶点索引偏移
        std::inclusive_scan(shape.mesh.num_face_vertices.begin(), shape.mesh.num_face_vertices.end(),
            std::back_inserter(mesh_data.face_vertex_offsets), std::plus<>(), mesh_data.face_vertex_offsets.back());

        int patch_id {0};
        try {
            patch_id = std::stoi(shape.name);
        } catch (const std::invalid_argument&) {
            spdlog::warn("Non-integer group name '{}' found in OBJ file, defaulting to patch ID 0.", shape.name);
        }
        
        auto& patch = mesh_data.patches_[patch_id];
        if (!patch) {
            patch = std::make_unique<Patch>(patch_id, patch_id);
        }
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            patch->faces.push_back(f + new_face);
        }

        auto& block = mesh_data.blocks_[patch_id];
        if (!block) {
            block = std::make_unique<Block>();
			block->id = patch_id;
			block->patchIDs.insert(patch_id);
        }
    }

    return result;
}

void ObjMeshIO::saveToFile(const MeshData& mesh, std::ostream& os)
{
    if (!os) {
        spdlog::error("Failed to open output OBJ file stream.");

    }

    os << "# Exported by MeshData\n";
    for (size_t v = 0; v < mesh.vertex_positions.size(); v++) {
        const auto& pos = mesh.vertex_positions[v];
        os << fmt::format("v {} {} {}\n", pos[0], pos[1], pos[2]);
    }
    // Loop over faces
    for (const auto& [patch_id, patch_ptr] : mesh.patches_) {
        os << fmt::format("g {}\n", patch_id); // group name as patch id
        const auto& patch = *patch_ptr;
        for (const auto& f : patch.faces) {
            os << "f";
            for (Index vi = mesh.face_vertex_offsets[f]; vi < mesh.face_vertex_offsets[f + 1]; vi++) {
                os << fmt::format(" {}", mesh.face_vertices[vi] + 1); // OBJ format uses 1-based index
            }
            os << "\n";
        }
    }
}
