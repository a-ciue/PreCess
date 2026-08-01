/**
 * @file InpModelHandler.cpp
 * @brief 使用mesh库实现的inp文件读写功能，侧重于实现写出功能
 *
 */
#include "InpModelHandler.h"
#include "ArgType.h"
#include "MeshData.h"
#include "ComponentData.h"
#include "ModelLayer.h"

#include <spdlog/spdlog.h>

#include "AbaqusPrecessConverter.h"
#include "abaqus_io.h"
#include <fstream>
#include <memory>
#include <vector>

namespace systems::io {

std::optional<ModelPayload> InpModelHandler::read_model(const fs::path& path, const std::vector<std::any>& args)
{
    try {
        if (path.string().find("..") != std::string::npos) {
            spdlog::error("InpModelHandler::read_model: relative paths with .. are not allowed");
            return std::nullopt;
        }
        Mesh_meshIO abaqus_mesh = read_abaqus(path.string());
        auto mesh = std::make_unique<MeshData>();
        mesh->init();
        convert_abaqus_to_meshdata(abaqus_mesh, *mesh);

        auto c = std::make_unique<ComponentData>();
        c->id = -1;
        c->name = "Comp_0";
        c->mesh = std::move(mesh);

        ComponentDatas comps;
        comps.push_back(std::move(c));

        spdlog::debug("read_model: point_sets.size() = {}", abaqus_mesh.point_sets.size());
        spdlog::debug("read_model: cell_sets.size() = {}", abaqus_mesh.cell_sets.size());
        return ModelPayload{path.filename().string(), std::move(comps)};
    } catch (const std::exception& e) {
        spdlog::error("InpModelHandler::read_model: exception reading {}: {}", path.string(), e.what());
        return std::nullopt;
    }
}

void InpModelHandler::write_components(const ModelLayer& mgr,
    const std::vector<Index>& component_ids,
    const fs::path& path,
    const std::vector<std::any>& /*args*/)
{
    try {
        if (path.string().find("..") != std::string::npos) {
            spdlog::error("InpModelHandler::write_components: relative paths with .. are not allowed");
            return;
        }
        if (component_ids.empty()) {
            spdlog::error("InpModelHandler::write_components called with empty component_ids");
            return;
        }

        Mesh_meshIO abaqus_mesh;

        size_t total_vertex_count = 0;

        for (Index cid : component_ids) {
            const ComponentData* comp = mgr.findComponent(cid);
            if (!comp) {
                spdlog::warn("InpModelHandler: component {} not found, skip", cid);
                continue;
            }
            if (!comp->mesh) {
                spdlog::warn("InpModelHandler: component {} has no mesh, skip", cid);
                continue;
            }

            const MeshData& m = *comp->mesh;
            const Index cnt = m.vertex_count_;
            if (cnt <= 0) {
                spdlog::warn("InpModelHandler: component {} has no vertices, skip", cid);
                continue;
            }

            // MeshData 自包含：坐标直接拷贝；连通性为组件内局部点索引，
            // 文件序号 = 本组件基址 vertex_base + 局部 id
            const Index vertex_base = static_cast<Index>(total_vertex_count);
            for (Index i = 0; i < cnt; ++i) {
                const auto& p = m.vertex_positions_[static_cast<size_t>(i)];
                abaqus_mesh.points.push_back({ p[0], p[1], p[2] });
            }
            total_vertex_count += cnt;

            if (!m.solid_vertices_offset_.empty()) {
                const auto& offsets = m.solid_vertices_offset_;
                size_t nsolids = offsets.size() > 0 ? offsets.size() - 1 : 0;
                std::unordered_map<std::string, size_t> block_index_by_type;

                for (size_t i = 0; i < nsolids; ++i) {
                    Index start = offsets[i];
                    Index end = offsets[i + 1];
                    std::vector<int> nodes;
                    nodes.reserve(end - start);
                    for (Index k = start; k < end; ++k) {
                        nodes.push_back(static_cast<int>(vertex_base + m.solid_vertices_[static_cast<size_t>(k)]));
                    }

                    unsigned char vtk_type = 0;
                    if (i < m.solid_types_.size())
                        vtk_type = m.solid_types_[i];
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

            if (!m.face_vertices_offset_.empty()) {
                const auto& foffsets = m.face_vertices_offset_;
                size_t nfaces = foffsets.size() > 0 ? foffsets.size() - 1 : 0;
                if (nfaces > 0) {
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
                            type = "CPE6";

                        std::vector<int> nodes;
                        nodes.reserve(cnt);
                        for (Index k = s; k < e; ++k) {
                            nodes.push_back(static_cast<int>(vertex_base + m.face_vertices_[static_cast<size_t>(k)]));
                        }

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

            if (!m.edge_vertices_.empty()) {
                size_t nedges = m.edge_vertices_.size() / 2;
                if (nedges > 0) {
                    std::string type = "line";
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
                        nodes.push_back(static_cast<int>(vertex_base + m.edge_vertices_[2 * i]));
                        nodes.push_back(static_cast<int>(vertex_base + m.edge_vertices_[2 * i + 1]));
                        abaqus_mesh.cells[idx].data.push_back(std::move(nodes));
                    }
                }
            }
        }

        write_abaqus(path.string(), abaqus_mesh);
        spdlog::info("InpModelHandler::write_components: wrote Abaqus .inp to {}", path.string());
        spdlog::info("write_components: abaqus_mesh.point_sets.size() = {}", abaqus_mesh.point_sets.size());
    } catch (const std::exception& e) {
        spdlog::error("InpModelHandler::write_components: exception writing {}: {}", path.string(), e.what());
    }
}

std::vector<core::ArgType> InpModelHandler::read_args_type() const
{
    return {};
}

std::vector<core::ArgType> InpModelHandler::write_args_type() const
{
    return {};
}

} // namespace systems::io