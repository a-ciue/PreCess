/**
 * @file PlyModelHandler_ai.cpp
 * @brief 使用tinyply库实现的PLY文件读写功能
 *        支持ASCII和二进制格式的PLY文件
 */
#include "PlyModelHandler.h"
#include "ArgType.h"
#include "MeshData.h"
#include "ModelData.h"

#define TINYPLY_IMPLEMENTATION
#include <tinyply.h>

#include <fstream>
#include <vector>
#include <memory>

namespace systems::io {

std::unique_ptr<ModelData> PlyModelHandler::read_model(const fs::path& path, const std::vector<std::any>& args)
{
    try {
        // 打开文件流
        std::ifstream ss(path, std::ios::binary);
        if (!ss) {
            std::fprintf(stderr, "PlyModelHandler: cannot open file %s\n", path.string().c_str());
            return {};
        }

        // 创建tinyply解析器
        tinyply::PlyFile file;
        if (!file.parse_header(ss)) {
            std::fprintf(stderr, "PlyModelHandler: malformed PLY header in %s\n", path.string().c_str());
            return {};
        }

        // 请求顶点数据 (x, y, z)
        auto vertices = file.request_properties_from_element("vertex", { "x", "y", "z" });
        
        // 请求面数据 (vertex_indices)
        auto faces = file.request_properties_from_element("face", { "vertex_indices" });

        // 读取数据
        file.read(ss);

        // 创建网格数据
        auto mesh = std::make_unique<MeshData>();
        mesh->init();

        // 处理顶点数据
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

        // 处理面数据（改进：支持不同的 count 类型和 index 类型，使用 memcpy 读取）
        if (faces && faces->count > 0) {
            mesh->face_vertices_offset_.clear();
            mesh->face_vertices_offset_.push_back(0);

            // 从 header 中获取 face.vertex_indices 的元信息（isList / listCount / listType / propertyType）
            bool prop_is_list = false;
            size_t prop_list_count = 0;
            tinyply::Type prop_list_count_type = tinyply::Type::INVALID; // count 的类型
            tinyply::Type prop_elem_type = faces->t; // 元素类型（索引的类型），faces->t 已由 request_properties_from_element 设置

            {
                auto elements = file.get_elements();
                for (const auto& elem : elements) {
                    if (elem.name == "face") {
                        for (const auto& prop : elem.properties) {
                            if (prop.name == "vertex_indices") {
                                prop_is_list = prop.isList;
                                prop_list_count = prop.listCount;
                                prop_list_count_type = prop.listType;
                                // 如果 request_properties_from_element 已经设置了类型，则以 faces->t 为准
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
                // 变长列表：tinyply 的 buffer 中仅包含元素数据（不包含 count），
                // 所以我们使用 faces->listSizes（由 tinyply 在读取时填充）来获取每个 face 的顶点数量。
                if (faces->listSizes.size() == total_faces) {
                    // 正常情况：使用 tinyply 填充的 per-face listSizes
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
                                // 不支持的类型，跳过一个元素宽度（尽量使用 faces->t 的 stride）
                                byte_offset += tinyply::PropertyTable[faces->t].stride;
                                break;
                            }
                        }
                        mesh->face_vertices_offset_.push_back(static_cast<Index>(mesh->face_vertices_.size()));
                    }
                } else {
                    // 回退：如果 tinyply 未填充 listSizes，尝试直接从 buffer 中按 header 的 countType 读取每个面的 count（兼容老实现）
                    std::fprintf(stderr, "PlyModelHandler: face listSizes not available for %s, falling back to buffer counts\n", path.string().c_str());
                    // 重置读取偏移
                    size_t testOffset = 0;
                    bool ok = true;
                    try {
                        for (size_t f = 0; f < total_faces; ++f) {
                            uint32_t vertex_count = 0;
                            // read_count using prop_list_count_type but from buffer/testOffset
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
                        std::fprintf(stderr, "PlyModelHandler: fallback parsing failed for %s: %s\n", path.string().c_str(), e.what());
                    }
                    if (!ok) {
                        // If fallback failed, abort reading faces
                        std::fprintf(stderr, "PlyModelHandler: cannot parse faces for %s\n", path.string().c_str());
                    }
                }
            } else if (prop_is_list && prop_list_count > 0) {
                // 固定长度列表：每个 face 按 prop_list_count 个元素排列（没有 count 字段）
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
                std::fprintf(stderr, "PlyModelHandler: cannot determine face vertex count/type for %s\n", path.string().c_str());
            }
        }

        auto model_data = std::make_unique<ModelData>(std::move(mesh));
        model_data->model_name_ = path.filename().string();
        return model_data;

    } catch (const std::exception& e) {
        std::fprintf(stderr, "PlyModelHandler: error reading %s: %s\n", path.string().c_str(), e.what());
        return {};
    }
}

void PlyModelHandler::write_model(const ModelData& data, const fs::path& path, const std::vector<std::any>& args)
{
    try {
        auto mesh_data = data.asMeshData();
        if (!mesh_data) {
            std::fprintf(stderr, "PlyModelHandler: only mesh data is supported for writing\n");
            return;
        }

        const auto& verts = mesh_data->vertex_positions_;
        const auto& offsets = mesh_data->face_vertices_offset_;
        const auto& faces = mesh_data->face_vertices_;

        size_t vertex_count = verts.size();
        size_t face_count = offsets.size() > 1 ? offsets.size() - 1 : 0;

        // 创建tinyply文件
        tinyply::PlyFile file;

        // 准备顶点数据
        std::vector<float> vertices_float;
        vertices_float.reserve(vertex_count * 3);
        for (const auto& v : verts) {
            vertices_float.push_back(static_cast<float>(v[0]));
            vertices_float.push_back(static_cast<float>(v[1]));
            vertices_float.push_back(static_cast<float>(v[2]));
        }

        // 准备面数据
        std::vector<int32_t> face_indices;
        std::vector<int32_t> face_vertexs;
        for (size_t fi = 0; fi < face_count; ++fi) {
            Index start = offsets[fi];
            Index end = offsets[fi + 1];
            Index nv = end - start;
            
            face_indices.push_back(static_cast<int32_t>(nv)); // 顶点数量
            //face_vertexs_count.push_back(static_cast<uint8_t>(nv));
            for (Index k = start; k < end; ++k) {
                face_indices.push_back(static_cast<int32_t>(faces[k])); // 顶点索引
                face_vertexs.push_back(static_cast<int32_t>(faces[k]));
            }
        }

        // 添加顶点属性
        file.add_properties_to_element("vertex", { "x", "y", "z" }, 
            tinyply::Type::FLOAT32, vertex_count, 
            reinterpret_cast<uint8_t*>(vertices_float.data()), 
            tinyply::Type::INVALID, 0);

        // 添加面属性
        
        file.add_properties_to_element("face", { "vertex_indices" }, 
            tinyply::Type::INT32, face_count, 
            reinterpret_cast<uint8_t*>(face_indices.data()), 
            tinyply::Type::INT8,0);
        
        /*
        file.add_properties_to_element("face", { "vertex_indices" },
            tinyply::Type::INT32, face_count,
            reinterpret_cast<uint8_t*>(face_vertexs.data()),
            tinyply::Type::INT8, 4);
        */
        // 写入文件
        std::ofstream ofs(path, std::ios::binary);
        if (!ofs) {
            std::fprintf(stderr, "PlyModelHandler: cannot create file %s\n", path.string().c_str());
            return;
        }

        // 默认使用ascii格式写入
        file.write(ofs, false);

    } catch (const std::exception& e) {
        std::fprintf(stderr, "PlyModelHandler: error writing %s: %s\n", path.string().c_str(), e.what());
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