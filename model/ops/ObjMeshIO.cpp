#include "OBJMeshIO.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <spdlog/spdlog.h>
#include <unordered_map>

std::optional<ComponentDatas> ObjMeshIO::loadFromFile(const std::filesystem::path& filename)
{
    tinyobj::ObjReaderConfig reader_config;
    reader_config.mtl_search_path = "."; // Path to material files
    reader_config.triangulate = false; // Do not triangulate faces

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(filename.string(), reader_config)) {
        if (!reader.Error().empty()) {
            spdlog::error("TinyObjReader: " + reader.Error());
        }
        return std::nullopt;
    }

    if (!reader.Warning().empty()) {
        spdlog::warn("TinyObjReader: " + reader.Warning());
    }

    auto& attrib = reader.GetAttrib();
    auto& shapes = reader.GetShapes();

    // 每个 shape(group) 拆为一个组件，MeshData 自包含：
    // 仅携带本组面引用的顶点，全局点索引重映射为组件内局部点索引
    ComponentDatas result;
    for (auto& shape : shapes) {
        if (shape.mesh.num_face_vertices.empty()) {
            continue; // 跳过没有面的组
        }

        auto mesh_data = std::make_unique<MeshData>();
        mesh_data->face_vertices_offset_.push_back(0);

        std::unordered_map<int, Index> global_to_local; // OBJ 全局点索引 -> 组件内局部点索引
        size_t corner = 0;
        for (unsigned char face_vertex_count : shape.mesh.num_face_vertices) {
            for (size_t k = 0; k < face_vertex_count; k++, corner++) {
                const int global_index = shape.mesh.indices[corner].vertex_index;
                if (global_index < 0) {
                    continue; // 无点索引的角点（理论上 OBJ 面不会缺点索引）
                }
                auto [it, inserted] = global_to_local.try_emplace(global_index,
                    static_cast<Index>(mesh_data->vertex_positions_.size()));
                if (inserted) {
                    mesh_data->vertex_positions_.push_back({ attrib.vertices[3 * global_index + 0],
                        attrib.vertices[3 * global_index + 1],
                        attrib.vertices[3 * global_index + 2] });
                }
                mesh_data->face_vertices_.push_back(it->second);
            }
            mesh_data->face_vertices_offset_.push_back(static_cast<Index>(mesh_data->face_vertices_.size()));
        }
        mesh_data->vertex_count_ = static_cast<Index>(mesh_data->vertex_positions_.size());

        auto component = std::make_unique<ComponentData>();
        component->id = -1; // 组件 id 由系统入池时分配
        component->name = shape.name.empty()
            ? "Comp_" + std::to_string(result.size())
            : shape.name; // 组名作为组件名
        component->mesh = std::move(mesh_data);
        result.push_back(std::move(component));
    }

    return result;
}

namespace {
//! @brief 写出网格的点与面（面点索引 = 1-based 偏移 + 局部点索引）
void writeMeshData(const MeshData& mesh, std::ostream& os, Index v_offset_1based)
{
    for (size_t v = 0; v < mesh.vertex_positions_.size(); v++) {
        const auto& pos = mesh.vertex_positions_[v];
        os << fmt::format("v {} {} {}\n", pos[0], pos[1], pos[2]);
    }
    // Loop over faces
    if (mesh.face_vertices_offset_.size() >= 2) {
        const Index face_count = static_cast<Index>(mesh.face_vertices_offset_.size() - 1);
        for (Index f = 0; f < face_count; f++) {
            const Index a = mesh.face_vertices_offset_[f];
            const Index b = mesh.face_vertices_offset_[f + 1];
            if (a < 0 || b < a || b > static_cast<Index>(mesh.face_vertices_.size())) {
                continue; // 跳过越界的脏面
            }
            os << "f";
            for (Index vi = a; vi < b; vi++) {
                os << fmt::format(" {}", v_offset_1based + mesh.face_vertices_[vi]);
            }
            os << "\n";
        }
    }
}
} // namespace

void ObjMeshIO::saveToFile(const MeshData& mesh, std::ostream& os)
{
    if (!os) {
        spdlog::error("Failed to open output OBJ file stream.");
        return;
    }

    os << "# Exported by MeshData\n";
    writeMeshData(mesh, os, 1);
}

void ObjMeshIO::saveToFile(const ComponentDatas& components, std::ostream& os)
{
    std::vector<const ComponentData*> views;
    views.reserve(components.size());
    for (const auto& component : components) {
        views.push_back(component.get());
    }
    saveToFile(views, os);
}

void ObjMeshIO::saveToFile(const std::vector<const ComponentData*>& components, std::ostream& os)
{
    if (!os) {
        spdlog::error("Failed to open output OBJ file stream.");
        return;
    }

    os << "# Exported by MeshData\n";

    // OBJ 点索引为 1-based 且全文件连续，需跨组件累加偏移
    Index v_offset_1based = 1;
    size_t comp_index = 0;
    for (const auto& component : components) {
        if (!component || !component->mesh) {
            continue; // 跳过无网格的组件
        }
        const MeshData& mesh = *component->mesh;

        // 组件名作为 OBJ object 名，loadFromFile 读回时按 shape 拆回组件
        os << "o " << (component->name.empty() ? "component_" + std::to_string(comp_index) : component->name) << "\n";
        comp_index++;

        writeMeshData(mesh, os, v_offset_1based);
        v_offset_1based += static_cast<Index>(mesh.vertex_positions_.size());
    }
}
