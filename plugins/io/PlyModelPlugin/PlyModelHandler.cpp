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
#include <unordered_map>

namespace systems::io {

template<typename T>
void parse_vertices(const uint8_t* buffer, size_t count, std::vector<std::array<double, 3>>& out)
{
    const T* verts = reinterpret_cast<const T*>(buffer);
    for (size_t i = 0; i < count; ++i) {
        out.push_back({
            static_cast<double>(verts[i * 3]),
            static_cast<double>(verts[i * 3 + 1]),
            static_cast<double>(verts[i * 3 + 2])
        });
    }
}

template<typename T>
Index parse_index(const uint8_t* src, size_t& offset)
{
    T v = 0;
    std::memcpy(&v, src + offset, sizeof(T));
    offset += sizeof(T);
    return static_cast<Index>(v);
}

template<typename T>
Index parse_index_with_check(const uint8_t* src, size_t& offset, size_t buffer_size)
{
    if (offset + sizeof(T) > buffer_size)
        throw std::runtime_error("PlyModelHandler: unexpected EOF while parsing face buffer");
    return parse_index<T>(src, offset);
}

template<typename T>
uint32_t parse_vertex_count(const uint8_t* src, size_t& offset)
{
    T v = 0;
    std::memcpy(&v, src + offset, sizeof(T));
    offset += sizeof(T);
    return static_cast<uint32_t>(static_cast<int32_t>(v));
}

template<>
uint32_t parse_vertex_count<uint8_t>(const uint8_t* src, size_t& offset)
{
    uint8_t v = 0;
    std::memcpy(&v, src + offset, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    return v;
}

template<>
uint32_t parse_vertex_count<uint32_t>(const uint8_t* src, size_t& offset)
{
    uint32_t v = 0;
    std::memcpy(&v, src + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    return v;
}

template<typename T>
uint32_t parse_vertex_count_with_check(const uint8_t* src, size_t& offset, size_t buffer_size)
{
    if (offset + sizeof(T) > buffer_size)
        throw std::runtime_error("PlyModelHandler: unexpected EOF while parsing face buffer");
    return parse_vertex_count<T>(src, offset);
}

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
            
            switch (vertices->t) {
                case tinyply::Type::FLOAT32: parse_vertices<float>(vertices->buffer.get(), vertices->count, mesh->vertex_positions_); break;
                case tinyply::Type::FLOAT64: parse_vertices<double>(vertices->buffer.get(), vertices->count, mesh->vertex_positions_); break;
                case tinyply::Type::INT8: parse_vertices<int8_t>(vertices->buffer.get(), vertices->count, mesh->vertex_positions_); break;
                case tinyply::Type::UINT8: parse_vertices<uint8_t>(vertices->buffer.get(), vertices->count, mesh->vertex_positions_); break;
                case tinyply::Type::INT16: parse_vertices<int16_t>(vertices->buffer.get(), vertices->count, mesh->vertex_positions_); break;
                case tinyply::Type::UINT16: parse_vertices<uint16_t>(vertices->buffer.get(), vertices->count, mesh->vertex_positions_); break;
                case tinyply::Type::INT32: parse_vertices<int32_t>(vertices->buffer.get(), vertices->count, mesh->vertex_positions_); break;
                case tinyply::Type::UINT32: parse_vertices<uint32_t>(vertices->buffer.get(), vertices->count, mesh->vertex_positions_); break;
                default:
                    spdlog::warn("PlyModelHandler: unsupported vertex property type");
                    break;
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

            if (prop_is_list && prop_list_count == 0) {
                if (faces->listSizes.size() == total_faces) {
                    for (size_t f = 0; f < total_faces; ++f) {
                        size_t vertex_count = faces->listSizes[f];
                        for (size_t k = 0; k < vertex_count; ++k) {
                            Index idx;
                            switch (prop_elem_type) {
                            case tinyply::Type::INT8: idx = parse_index_with_check<int8_t>(src, byte_offset, bufferSize); break;
                            case tinyply::Type::UINT8: idx = parse_index_with_check<uint8_t>(src, byte_offset, bufferSize); break;
                            case tinyply::Type::INT16: idx = parse_index_with_check<int16_t>(src, byte_offset, bufferSize); break;
                            case tinyply::Type::UINT16: idx = parse_index_with_check<uint16_t>(src, byte_offset, bufferSize); break;
                            case tinyply::Type::INT32: idx = parse_index_with_check<int32_t>(src, byte_offset, bufferSize); break;
                            case tinyply::Type::UINT32: idx = parse_index_with_check<uint32_t>(src, byte_offset, bufferSize); break;
                            default:
                                byte_offset += tinyply::PropertyTable[faces->t].stride;
                                continue;
                            }
                            mesh->face_vertices_.push_back(idx);
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
                            case tinyply::Type::UINT8: vertex_count = parse_vertex_count_with_check<uint8_t>(src, testOffset, bufferSize); break;
                            case tinyply::Type::INT8: vertex_count = parse_vertex_count_with_check<int8_t>(src, testOffset, bufferSize); break;
                            case tinyply::Type::UINT16: vertex_count = parse_vertex_count_with_check<uint16_t>(src, testOffset, bufferSize); break;
                            case tinyply::Type::INT16: vertex_count = parse_vertex_count_with_check<int16_t>(src, testOffset, bufferSize); break;
                            case tinyply::Type::UINT32: vertex_count = parse_vertex_count_with_check<uint32_t>(src, testOffset, bufferSize); break;
                            case tinyply::Type::INT32: vertex_count = parse_vertex_count_with_check<int32_t>(src, testOffset, bufferSize); break;
                            default: vertex_count = parse_vertex_count_with_check<uint8_t>(src, testOffset, bufferSize); break;
                            }

                            for (uint32_t k = 0; k < vertex_count; ++k) {
                                Index idx;
                                switch (prop_elem_type) {
                                case tinyply::Type::INT8: idx = parse_index_with_check<int8_t>(src, testOffset, bufferSize); break;
                                case tinyply::Type::UINT8: idx = parse_index_with_check<uint8_t>(src, testOffset, bufferSize); break;
                                case tinyply::Type::INT16: idx = parse_index_with_check<int16_t>(src, testOffset, bufferSize); break;
                                case tinyply::Type::UINT16: idx = parse_index_with_check<uint16_t>(src, testOffset, bufferSize); break;
                                case tinyply::Type::INT32: idx = parse_index_with_check<int32_t>(src, testOffset, bufferSize); break;
                                case tinyply::Type::UINT32: idx = parse_index_with_check<uint32_t>(src, testOffset, bufferSize); break;
                                default:
                                    throw std::runtime_error("PlyModelHandler: unsupported face index type in fallback");
                                }
                                mesh->face_vertices_.push_back(idx);
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
                        Index idx;
                        switch (prop_elem_type) {
                        case tinyply::Type::INT8: idx = parse_index<int8_t>(src, byte_offset); break;
                        case tinyply::Type::UINT8: idx = parse_index<uint8_t>(src, byte_offset); break;
                        case tinyply::Type::INT16: idx = parse_index<int16_t>(src, byte_offset); break;
                        case tinyply::Type::UINT16: idx = parse_index<uint16_t>(src, byte_offset); break;
                        case tinyply::Type::INT32: idx = parse_index<int32_t>(src, byte_offset); break;
                        case tinyply::Type::UINT32: idx = parse_index<uint32_t>(src, byte_offset); break;
                        default:
                            byte_offset += tinyply::PropertyTable[faces->t].stride;
                            continue;
                        }
                        mesh->face_vertices_.push_back(idx);
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

            std::unordered_map<Index, Index> global_to_local;
            global_to_local.reserve(static_cast<size_t>(cnt));

            for (Index i = 0; i < cnt; ++i) {
                if (i >= static_cast<Index>(m.local_to_global_.size())) {
                    spdlog::error("PlyModelHandler: local_to_global size mismatch, cid={}", cid);
                    return;
                }
                const Index gid = m.local_to_global_[i];
                if (gid < 0 || static_cast<size_t>(gid) >= gp.size()) {
                    spdlog::error("PlyModelHandler: global point id out of range, cid={}, gid={}", cid, gid);
                    return;
                }
                global_to_local[gid] = i;
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
                        auto it = global_to_local.find(gid);
                        if (it == global_to_local.end()) {
                            spdlog::error("PlyModelHandler: face references vertex not in component, cid={}, gid={}", cid, gid);
                            return;
                        }
                        all_face_indices.push_back(static_cast<int32_t>(total_vertex_count + it->second));
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

        ofs.flush();
        if (ofs.fail()) {
            spdlog::error("PlyModelHandler: write failed for {}", path.string().c_str());
            throw std::runtime_error("Failed to write PLY file");
        }

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
