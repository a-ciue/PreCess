/**
 * @file PlyModelHandler.cpp
 * @brief 使用tinyply库实现的PLY文件读写功能
 *        
 */
#include "PlyModelHandler.h"
#include "ArgType.h"
#include "MeshData.h"
#include "ModelData.h"
#include "ModelManager.h"

#define TINYPLY_IMPLEMENTATION
#include <tinyply.h>
#include <spdlog/spdlog.h>

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
            spdlog::error("PlyModelHandler: cannot open file {}", path.string().c_str());
            return {};
        }

        // 创建tinyply解析器
        tinyply::PlyFile file;
        if (!file.parse_header(ss)) {
            spdlog::error("PlyModelHandler: malformed PLY header in {}", path.string().c_str());
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
                    spdlog::error("PlyModelHandler: face listSizes not available for {}, falling back to buffer counts", path.string().c_str());
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
                        spdlog::error("PlyModelHandler: fallback parsing failed for {}: {}", path.string().c_str(), e.what());
                    }
                    if (!ok) {
                        // If fallback failed, abort reading faces
                        spdlog::error("PlyModelHandler: cannot parse faces for {}", path.string().c_str());
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
                spdlog::error("PlyModelHandler: cannot determine face vertex count/type for {}", path.string().c_str());
            }
        }

        auto model_data = std::make_unique<ModelData>(std::move(mesh));
        model_data->model_name_ = path.filename().string();
        return model_data;

    } catch (const std::exception& e) {
        spdlog::error("PlyModelHandler: error reading {}: {}", path.string().c_str(), e.what());
        return {};
    }
}

void PlyModelHandler::write_components(const ModelManager& mgr,
    const std::vector<Index>& component_ids,
    const fs::path& path,
    const std::vector<std::any>& /*args*/)
{
    if (component_ids.empty()) {
        spdlog::error("PlyModelHandler::write_components: empty component_ids");
        return;
    }

    const auto& gp = mgr.globalPoints();

    // 1) 统计总顶点数/总面数，并确定最大面点数（用于选择 list count 类型）
    Index total_vertices = 0;
    Index total_faces = 0;
    Index max_face_n = 0;

    struct CompExportInfo {
        const Component* comp {};
        const MeshData* mesh {};
        Index base {};
        Index cnt {};
        Index face_count {};
    };
    std::vector<CompExportInfo> infos;
    infos.reserve(component_ids.size());

    for (Index cid : component_ids) {
        const Component* comp = mgr.findComponent(cid);
        if (!comp || !comp->mesh) {
            spdlog::warn("PlyModelHandler: component {} missing or no mesh, skip", cid);
            continue;
        }
        const MeshData& m = *comp->mesh;
        const Index base = m.global_point_base_;
        const Index cnt = m.vertex_count_;

        if (base < 0 || cnt <= 0 || base + cnt > (Index)gp.size()) {
            spdlog::error("PlyModelHandler: invalid global point range, cid={}, base={}, cnt={}, gp={}",
                cid, base, cnt, gp.size());
            continue;
        }

        const Index face_count = (m.face_vertices_offset_.size() >= 2)
            ? (Index)m.face_vertices_offset_.size() - 1
            : 0;

        // 统计 max_face_n
        if (face_count > 0) {
            for (Index f = 0; f < face_count; ++f) {
                Index a = m.face_vertices_offset_[(size_t)f];
                Index b = m.face_vertices_offset_[(size_t)f + 1];
                if (a < 0 || b < a || b > (Index)m.face_vertices_.size())
                    continue;
                max_face_n = std::max(max_face_n, b - a);
            }
        }

        infos.push_back({ comp, &m, base, cnt, face_count });

        total_vertices += cnt;
        total_faces += face_count;
    }

    if (total_vertices <= 0) {
        spdlog::error("PlyModelHandler: no vertices to export");
        return;
    }

    // PLY list count 类型：若有面顶点数 >255，用 uint（这里用 uint32_t 表达）
    const bool use_uint_count = (max_face_n > 255);

    // 2) 写 header
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) {
        spdlog::error("PlyModelHandler: cannot open output {}", path.string());
        return;
    }

    ofs << "ply\n";
    ofs << "format ascii 1.0\n";
    ofs << "comment generated by PreCess\n";
    ofs << "element vertex " << total_vertices << "\n";
    // 用 double，保证 round-trip 精度；你的读逻辑支持 FLOAT64
    ofs << "property double x\n";
    ofs << "property double y\n";
    ofs << "property double z\n";
    ofs << "element face " << total_faces << "\n";
    ofs << "property list " << (use_uint_count ? "uint" : "uchar") << " int vertex_indices\n";
    ofs << "end_header\n";

    // 3) 写 vertices（按 component 顺序拼接）
    ofs << std::setprecision(17); // double 足够高精度
    for (const auto& info : infos) {
        for (Index i = 0; i < info.cnt; ++i) {
            const auto& p = gp[(size_t)(info.base + i)];
            ofs << p[0] << " " << p[1] << " " << p[2] << "\n";
        }
    }

    // 4) 写 faces（保留每个面的顶点数，不三角化）
    Index vertex_offset = 0; // 文件内顶点偏移（0-based）
    for (const auto& info : infos) {
        const MeshData& m = *info.mesh;
        const Index base = info.base;
        const Index cnt = info.cnt;

        for (Index f = 0; f < info.face_count; ++f) {
            Index a = m.face_vertices_offset_[(size_t)f];
            Index b = m.face_vertices_offset_[(size_t)f + 1];
            if (a < 0 || b < a || b > (Index)m.face_vertices_.size()) {
                // 写一个空 face 会破坏结构；这里直接跳过（但会导致 face 数不一致）
                // 更稳：在统计 total_faces 时就过滤非法 face；这里假设数据合法
                spdlog::error("PlyModelHandler: invalid face offset range f={}, a={}, b={}", f, a, b);
                throw std::runtime_error("PlyModelHandler: invalid face offsets");
            }

            Index n = b - a;
            if (use_uint_count)
                ofs << (uint32_t)n;
            else
                ofs << (uint32_t)n; // uchar 也用数值打印即可

            for (Index k = a; k < b; ++k) {
                Index gid = m.face_vertices_[(size_t)k]; // 全局点 id
                Index local = gid - base; // component 局部
                if (local < 0 || local >= cnt) {
                    spdlog::error("PlyModelHandler: face references vertex out of component range (gid={}, base={}, cnt={})",
                        gid, base, cnt);
                    throw std::runtime_error("PlyModelHandler: face index out of range");
                }
                Index file_vid = vertex_offset + local; // 文件内 0-based
                ofs << " " << (int)file_vid;
            }
            ofs << "\n";
        }

        vertex_offset += cnt;
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