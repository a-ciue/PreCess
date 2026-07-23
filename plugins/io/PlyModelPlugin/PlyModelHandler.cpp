/**
 * @file PlyModelHandler.cpp
 * @brief 使用tinyply库实现的PLY文件读写功能
 *        
 */
#include "PlyModelHandler.h"
#include "ArgType.h"
#include "MeshData.h"
#include "ComponentData.h"
#include "ModelLayer.h"

#define TINYPLY_IMPLEMENTATION
#include <tinyply.h>
#include <spdlog/spdlog.h>

#include <fstream>
#include <vector>
#include <memory>

namespace systems::io {

std::optional<ModelPayload> PlyModelHandler::read_model(const fs::path& path, const std::vector<std::any>& args)
{
    try {
        std::ifstream ss(path, std::ios::binary);
        if (!ss) {
            spdlog::error("PlyModelHandler: cannot open file {}", path.string().c_str());
            return std::nullopt;
        }

        tinyply::PlyFile file;
        if (!file.parse_header(ss)) {
            spdlog::error("PlyModelHandler: malformed PLY header in {}", path.string().c_str());
            return std::nullopt;
        }

        auto vertices = file.request_properties_from_element("vertex", { "x", "y", "z" });
        auto faces = file.request_properties_from_element("face", { "vertex_indices" });

        file.read(ss);

        auto mesh = std::make_unique<MeshData>();
        mesh->init();

        if (vertices && vertices->count > 0) {
            mesh->vertex_positions_.reserve(vertices->count);
            
            if (vertices->t == tinyply::Type::FLOAT32) {
                const float* verts = reinterpret_cast<const float*>(vertices->buffer.get());
                for (size_t i = 0; i < vertices->count; ++i) {
                    mesh->vertex_positions_.push_back({
                        static_cast<double>(verts[i * 3]),
                        static_cast<double>(verts[i * 3 + 1]), 
                        static_cast<double>(verts[i * 3 + 2])
                    });
                }
            } else if (vertices->t == tinyply::Type::FLOAT64) {
                const double* verts = reinterpret_cast<const double*>(vertices->buffer.get());
                for (size_t i = 0; i < vertices->count; ++i) {
                    mesh->vertex_positions_.push_back({
                        verts[i * 3],
                        verts[i * 3 + 1],
                        verts[i * 3 + 2]
                    });
                }
            }
        }

        if (faces && faces->count > 0) {
            mesh->face_vertices_offset_.clear();
            mesh->face_vertices_offset_.push_back(0);

            bool prop_is_list = false;
            size_t prop_list_count = 0;
            tinyply::Type prop_list_count_type = tinyply::Type::INVALID;
            tinyply::Type prop_elem_type = faces->t;

            {
                auto elements = file.get_elements();
                for (const auto& elem : elements) {
                    if (elem.name == "face") {
                        for (const auto& prop : elem.properties) {
                            if (prop.name == "vertex_indices") {
                                prop_is_list = prop.isList;
                                prop_list_count = prop.listCount;
                                prop_list_count_type = prop.listType;
                                if (faces->t == tinyply::Type::INVALID)
                                    prop_elem_type = prop.propertyType;
                                break;
                            }
                        }
                        break;
                    }
                }
            }

            const uint8_t* src = faces->buffer.get_const();
            const size_t total_faces = faces->count;
            size_t byte_offset = 0;
            const size_t bufferSize = faces->buffer.size_bytes();
            auto ensure_space_at = [&](size_t offset, size_t need) {
                if (offset + need > bufferSize)
                    throw std::runtime_error("PlyModelHandler: unexpected EOF while parsing face buffer");
            };

            if (prop_is_list && prop_list_count == 0) {
                if (faces->listSizes.size() == total_faces) {
                    for (size_t f = 0; f < total_faces; ++f) {
                        size_t vertex_count = faces->listSizes[f];
                        for (size_t k = 0; k < vertex_count; ++k) {
                            switch (prop_elem_type) {
                            case tinyply::Type::INT8: {
                                ensure_space_at(byte_offset, 1);
                                int8_t v = 0; std::memcpy(&v, src + byte_offset, sizeof(int8_t)); byte_offset += sizeof(int8_t); mesh->face_vertices_.push_back(static_cast<Index>(v)); } break;
                            case tinyply::Type::UINT8: {
                                ensure_space_at(byte_offset, 1);
                                uint8_t v = 0; std::memcpy(&v, src + byte_offset, sizeof(uint8_t)); byte_offset += sizeof(uint8_t); mesh->face_vertices_.push_back(static_cast<Index>(v)); } break;
                            case tinyply::Type::INT16: {
                                ensure_space_at(byte_offset, 2);
                                int16_t v = 0; std::memcpy(&v, src + byte_offset, sizeof(int16_t)); byte_offset += sizeof(int16_t); mesh->face_vertices_.push_back(static_cast<Index>(v)); } break;
                            case tinyply::Type::UINT16: {
                                ensure_space_at(byte_offset, 2);
                                uint16_t v = 0; std::memcpy(&v, src + byte_offset, sizeof(uint16_t)); byte_offset += sizeof(uint16_t); mesh->face_vertices_.push_back(static_cast<Index>(v)); } break;
                            case tinyply::Type::INT32: {
                                ensure_space_at(byte_offset, 4);
                                int32_t v = 0; std::memcpy(&v, src + byte_offset, sizeof(int32_t)); byte_offset += sizeof(int32_t); mesh->face_vertices_.push_back(static_cast<Index>(v)); } break;
                            case tinyply::Type::UINT32: {
                                ensure_space_at(byte_offset, 4);
                                uint32_t v = 0; std::memcpy(&v, src + byte_offset, sizeof(uint32_t)); byte_offset += sizeof(uint32_t); mesh->face_vertices_.push_back(static_cast<Index>(v)); } break;
                            default:
                                byte_offset += tinyply::PropertyTable[faces->t].stride;
                                break;
                            }
                        }
                        mesh->face_vertices_offset_.push_back(static_cast<Index>(mesh->face_vertices_.size()));
                    }
                } else {
                    spdlog::error("PlyModelHandler: face listSizes not available for {}, falling back to buffer counts", path.string().c_str());
                    size_t testOffset = 0;
                    bool ok = true;
                    try {
                        for (size_t f = 0; f < total_faces; ++f) {
                            uint32_t vertex_count = 0;
                            switch (prop_list_count_type) {
                            case tinyply::Type::UINT8: {
                                ensure_space_at(testOffset, 1);
                                uint8_t v; std::memcpy(&v, src + testOffset, 1); vertex_count = v; testOffset += 1; } break;
                            case tinyply::Type::INT8: {
                                ensure_space_at(testOffset, 1);
                                int8_t v; std::memcpy(&v, src + testOffset, 1); vertex_count = static_cast<uint32_t>(static_cast<int32_t>(v)); testOffset += 1; } break;
                            case tinyply::Type::UINT16: {
                                ensure_space_at(testOffset, 2);
                                uint16_t v; std::memcpy(&v, src + testOffset, 2); vertex_count = v; testOffset += 2; } break;
                            case tinyply::Type::INT16: {
                                ensure_space_at(testOffset, 2);
                                int16_t v; std::memcpy(&v, src + testOffset, 2); vertex_count = static_cast<uint32_t>(static_cast<int32_t>(v)); testOffset += 2; } break;
                            case tinyply::Type::UINT32: {
                                ensure_space_at(testOffset, 4);
                                uint32_t v; std::memcpy(&v, src + testOffset, 4); vertex_count = v; testOffset += 4; } break;
                            case tinyply::Type::INT32: {
                                ensure_space_at(testOffset, 4);
                                int32_t v; std::memcpy(&v, src + testOffset, 4); vertex_count = static_cast<uint32_t>(v); testOffset += 4; } break;
                            default: {
                                ensure_space_at(testOffset, 1);
                                uint8_t v; std::memcpy(&v, src + testOffset, 1); vertex_count = v; testOffset += 1; } break;
                            }

                            for (uint32_t k = 0; k < vertex_count; ++k) {
                                uint32_t idx = 0;
                                switch (prop_elem_type) {
                                case tinyply::Type::INT8: {
                                    ensure_space_at(testOffset, 1);
                                    int8_t vv; std::memcpy(&vv, src + testOffset, 1); idx = static_cast<uint32_t>(static_cast<int32_t>(vv)); testOffset += 1; } break;
                                case tinyply::Type::UINT8: {
                                    ensure_space_at(testOffset, 1);
                                    uint8_t vv; std::memcpy(&vv, src + testOffset, 1); idx = vv; testOffset += 1; } break;
                                case tinyply::Type::INT16: {
                                    ensure_space_at(testOffset, 2);
                                    int16_t vv; std::memcpy(&vv, src + testOffset, 2); idx = static_cast<uint32_t>(static_cast<int32_t>(vv)); testOffset += 2; } break;
                                case tinyply::Type::UINT16: {
                                    ensure_space_at(testOffset, 2);
                                    uint16_t vv; std::memcpy(&vv, src + testOffset, 2); idx = vv; testOffset += 2; } break;
                                case tinyply::Type::INT32: {
                                    ensure_space_at(testOffset, 4);
                                    int32_t vv; std::memcpy(&vv, src + testOffset, 4); idx = static_cast<uint32_t>(vv); testOffset += 4; } break;
                                case tinyply::Type::UINT32: {
                                    ensure_space_at(testOffset, 4);
                                    uint32_t vv; std::memcpy(&vv, src + testOffset, 4); idx = vv; testOffset += 4; } break;
                                default:
                                    throw std::runtime_error("PlyModelHandler: unsupported face index type in fallback");
                                }
                                mesh->face_vertices_.push_back(static_cast<Index>(idx));
                            }
                            mesh->face_vertices_offset_.push_back(static_cast<Index>(mesh->face_vertices_.size()));
                        }
                    } catch (const std::exception& e) {
                        ok = false;
                        spdlog::error("PlyModelHandler: fallback parsing failed for {}: {}", path.string().c_str(), e.what());
                    }
                    if (!ok) {
                        spdlog::error("PlyModelHandler: cannot parse faces for {}", path.string().c_str());
                    }
                }
            } else if (prop_is_list && prop_list_count > 0) {
                for (size_t f = 0; f < total_faces; ++f) {
                    for (size_t k = 0; k < prop_list_count; ++k) {
                        switch (prop_elem_type) {
                        case tinyply::Type::INT8: {
                            int8_t v = 0; std::memcpy(&v, src + byte_offset, sizeof(int8_t)); byte_offset += sizeof(int8_t); mesh->face_vertices_.push_back(static_cast<Index>(v)); } break;
                        case tinyply::Type::UINT8: {
                            uint8_t v = 0; std::memcpy(&v, src + byte_offset, sizeof(uint8_t)); byte_offset += sizeof(uint8_t); mesh->face_vertices_.push_back(static_cast<Index>(v)); } break;
                        case tinyply::Type::INT16: {
                            int16_t v = 0; std::memcpy(&v, src + byte_offset, sizeof(int16_t)); byte_offset += sizeof(int16_t); mesh->face_vertices_.push_back(static_cast<Index>(v)); } break;
                        case tinyply::Type::UINT16: {
                            uint16_t v = 0; std::memcpy(&v, src + byte_offset, sizeof(uint16_t)); byte_offset += sizeof(uint16_t); mesh->face_vertices_.push_back(static_cast<Index>(v)); } break;
                        case tinyply::Type::INT32: {
                            int32_t v = 0; std::memcpy(&v, src + byte_offset, sizeof(int32_t)); byte_offset += sizeof(int32_t); mesh->face_vertices_.push_back(static_cast<Index>(v)); } break;
                        case tinyply::Type::UINT32: {
                            uint32_t v = 0; std::memcpy(&v, src + byte_offset, sizeof(uint32_t)); byte_offset += sizeof(uint32_t); mesh->face_vertices_.push_back(static_cast<Index>(v)); } break;
                        default:
                            byte_offset += tinyply::PropertyTable[faces->t].stride;
                            break;
                        }
                    }
                    mesh->face_vertices_offset_.push_back(static_cast<Index>(mesh->face_vertices_.size()));
                }
            } else {
                spdlog::error("PlyModelHandler: cannot determine face vertex count/type for {}", path.string().c_str());
            }
        }

        auto c = std::make_unique<ComponentData>();
        c->id = -1;
        c->name = "Comp_0";
        c->mesh = std::move(mesh);

        ComponentDatas comps;
        comps.push_back(std::move(c));

        return ModelPayload{path.filename().string(), std::move(comps)};

    } catch (const std::exception& e) {
        spdlog::error("PlyModelHandler: error reading {}: {}", path.string().c_str(), e.what());
        return std::nullopt;
    }
}

void PlyModelHandler::write_components(const ModelLayer& mgr,
    const std::vector<Index>& component_ids,
    const fs::path& path,
    const std::vector<std::any>& /*args*/)
{
    try {
        if (component_ids.empty()) {
            spdlog::error("PlyModelHandler: write_components called with empty component_ids");
            return;
        }

        const auto& gp = mgr.globalPoints();

        std::vector<float> all_vertices;
        std::vector<int32_t> all_face_indices;
        size_t total_vertex_count = 0;

        for (Index cid : component_ids) {
            const ComponentData* comp = mgr.findComponent(cid);
            if (!comp) {
                spdlog::warn("PlyModelHandler: component {} not found, skip", cid);
                continue;
            }
            if (!comp->mesh) {
                spdlog::warn("PlyModelHandler: component {} has no mesh, skip", cid);
                continue;
            }

            const MeshData& m = *comp->mesh;
            const Index cnt = m.vertex_count_;
            if (cnt <= 0) {
                spdlog::warn("PlyModelHandler: component {} has no vertices, skip", cid);
                continue;
            }

            for (Index i = 0; i < cnt; ++i) {
                const Index gid = m.local_to_global_[i];
                const auto& p = gp[(size_t)gid];
                all_vertices.push_back(static_cast<float>(p[0]));
                all_vertices.push_back(static_cast<float>(p[1]));
                all_vertices.push_back(static_cast<float>(p[2]));
            }

            if (m.face_vertices_offset_.size() >= 2) {
                const Index nFaces = static_cast<Index>(m.face_vertices_offset_.size() - 1);
                for (Index f = 0; f < nFaces; ++f) {
                    const Index a = m.face_vertices_offset_[static_cast<size_t>(f)];
                    const Index b = m.face_vertices_offset_[static_cast<size_t>(f + 1)];
                    if (a < 0 || b < a || b > static_cast<Index>(m.face_vertices_.size()))
                        continue;

                    Index nv = b - a;
                    all_face_indices.push_back(static_cast<int32_t>(nv));

                    for (Index k = a; k < b; ++k) {
                        const Index gid = m.face_vertices_[static_cast<size_t>(k)];
                        Index local_idx = -1;
                        for (Index i = 0; i < cnt; ++i) {
                            if (m.local_to_global_[i] == gid) {
                                local_idx = i;
                                break;
                            }
                        }
                        if (local_idx == -1) {
                            spdlog::error("PlyModelHandler: face references vertex not in component, cid={}, gid={}", cid, gid);
                            return;
                        }
                        all_face_indices.push_back(static_cast<int32_t>(total_vertex_count + local_idx));
                    }
                }
            }

            total_vertex_count += cnt;
        }

        tinyply::PlyFile file;

        file.add_properties_to_element("vertex", { "x", "y", "z" },
            tinyply::Type::FLOAT32, total_vertex_count,
            reinterpret_cast<uint8_t*>(all_vertices.data()),
            tinyply::Type::INVALID, 0);

        size_t face_count = 0;
        size_t idx = 0;
        while (idx < all_face_indices.size()) {
            int32_t nv = all_face_indices[idx];
            idx += nv + 1;
            face_count++;
        }

        file.add_properties_to_element("face", { "vertex_indices" },
            tinyply::Type::INT32, face_count,
            reinterpret_cast<uint8_t*>(all_face_indices.data()),
            tinyply::Type::INT8, 0);

        std::ofstream ofs(path, std::ios::binary);
        if (!ofs) {
            spdlog::error("PlyModelHandler: cannot create file {}", path.string().c_str());
            return;
        }

        file.write(ofs, false);

    } catch (const std::exception& e) {
        spdlog::error("PlyModelHandler: error writing {}: {}", path.string().c_str(), e.what());
    }
}

std::vector<core::ArgType> PlyModelHandler::read_args_type() const
{
    return {};
}

std::vector<core::ArgType> PlyModelHandler::write_args_type() const
{
    return {};
}

} // namespace systems::io
