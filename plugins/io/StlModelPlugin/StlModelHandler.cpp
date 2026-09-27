/**
 * @file StlModelHandler.cpp
 * @brief STL 三角网格文件读写实现
 *
 * STL 为三角形 soup 格式（每个三角形独立携带 3 个顶点，无共享顶点）：
 * - ASCII：solid/facet normal/outer loop/vertex×3/endloop/endfacet/endsolid；
 * - 二进制：80 字节头 + uint32 三角形数 + 每三角形 50 字节
 *   （法向 3×float32 + 顶点 3×3×float32 + uint16 属性计数，均小端）。
 *
 * 读取后按坐标精确匹配焊接重复顶点，组装为共享顶点的三角形面网格；
 * 写出仅支持 ASCII（法向按顶点几何重算，退化面写 0）。
 */
#include "StlModelHandler.h"

#include "ArgType.h"
#include "ComponentData.h"
#include "MeshData.h"
#include "ModelData.h"
#include "ModelLayer.h"

#include <spdlog/spdlog.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

//! @brief 每个三角形面的顶点数
constexpr size_t kTriangleCornerCount = 3;
//! @brief 二进制 STL 头部字节数（80 字节说明 + 4 字节三角形数）
constexpr size_t kBinaryHeaderSize = 84;
//! @brief 二进制 STL 每个三角形的字节数（12×float32 + uint16）
constexpr size_t kBinaryFacetSize = 50;

//! @brief 去掉行首尾空白与行尾 '\r'（二进制模式下 CRLF 不做转换）
std::string trimLine(const std::string& line)
{
    const auto first = line.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = line.find_last_not_of(" \t\r\n");
    return line.substr(first, last - first + 1);
}

//! @brief 三角形角点坐标的精确匹配哈希（按 double 位模式混合，-0.0 归一化为 0.0）
struct PointHash {
    size_t operator()(const std::array<double, 3>& point) const noexcept
    {
        size_t seed = 0x9e3779b97f4a7c15ULL;
        for (double value : point) {
            if (value == 0.0) {
                value = 0.0; // +0.0 与 -0.0 数值相等，归一化后位模式一致
            }
            uint64_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            seed ^= bits + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

//! @brief 把一个三角形 soup 角点焊接进网格：相同坐标精确合并为共享顶点
void weldTriangle(const std::array<double, 3>* corners,
    std::unordered_map<std::array<double, 3>, Index, PointHash>& vertex_ids,
    MeshData& mesh)
{
    std::array<Index, kTriangleCornerCount> local_ids {};
    for (size_t c = 0; c < kTriangleCornerCount; ++c) {
        auto [it, inserted] = vertex_ids.emplace(corners[c],
            static_cast<Index>(mesh.vertex_positions_.size()));
        if (inserted) {
            mesh.vertex_positions_.push_back(corners[c]);
        }
        local_ids[c] = it->second;
    }
    mesh.face_vertices_.insert(mesh.face_vertices_.end(),
        local_ids.begin(), local_ids.end());
    mesh.face_vertices_offset_.push_back(static_cast<Index>(mesh.face_vertices_.size()));
}

//! @brief 解析 ASCII STL：关键字驱动收集 facet 的 3 个 vertex，非法记录整体失败
bool readAscii(std::istream& input, MeshData& mesh)
{
    mesh.init();

    std::unordered_map<std::array<double, 3>, Index, PointHash> vertex_ids;
    Index facet_count = 0;
    std::array<std::array<double, 3>, kTriangleCornerCount> corners {};
    size_t corner_seen = 0;

    std::string raw;
    while (std::getline(input, raw)) {
        const std::string line = trimLine(raw);
        if (line.empty()) {
            continue;
        }

        std::istringstream record(line);
        std::string keyword;
        record >> keyword;
        if (keyword == "vertex") {
            if (corner_seen >= kTriangleCornerCount) {
                spdlog::error("StlModelHandler: facet has more than {} vertices", kTriangleCornerCount);
                return false;
            }
            if (!(record >> corners[corner_seen][0] >> corners[corner_seen][1] >> corners[corner_seen][2])) {
                spdlog::error("StlModelHandler: bad vertex record '{}'", line);
                return false;
            }
            ++corner_seen;
        } else if (keyword == "endfacet") {
            if (corner_seen != kTriangleCornerCount) {
                spdlog::error("StlModelHandler: facet has {} vertex record(s), expected {}",
                    corner_seen, kTriangleCornerCount);
                return false;
            }
            weldTriangle(corners.data(), vertex_ids, mesh);
            ++facet_count;
            corner_seen = 0;
        } else if (keyword == "facet" || keyword == "outer" || keyword == "loop"
            || keyword == "endloop" || keyword == "solid" || keyword == "endsolid") {
            // 结构关键字不携带建模数据：facet 的法向字段写出侧重算，读入忽略
            continue;
        } else {
            spdlog::warn("StlModelHandler: unknown keyword '{}', line skipped", keyword);
        }
    }

    if (corner_seen != 0) {
        spdlog::error("StlModelHandler: truncated facet at end of file");
        return false;
    }
    if (facet_count == 0) {
        spdlog::error("StlModelHandler: no facet record found");
        return false;
    }

    mesh.vertex_count_ = static_cast<Index>(mesh.vertex_positions_.size());
    spdlog::info("StlModelHandler: read {} facets, {} welded vertices",
        facet_count, mesh.vertex_count_);
    return true;
}

//! @brief 解析二进制 STL：布局校验通过后逐三角形读取，坐标 float32 转 double
bool readBinary(const std::vector<uint8_t>& bytes, MeshData& mesh)
{
    mesh.init();

    uint32_t facet_count = 0;
    std::memcpy(&facet_count, bytes.data() + 80, sizeof(facet_count));
    if (kBinaryHeaderSize + static_cast<size_t>(facet_count) * kBinaryFacetSize != bytes.size()) {
        spdlog::error("StlModelHandler: binary size mismatch ({} facets declared, {} bytes total)",
            facet_count, bytes.size());
        return false;
    }

    std::unordered_map<std::array<double, 3>, Index, PointHash> vertex_ids;
    std::array<std::array<double, 3>, kTriangleCornerCount> corners {};
    for (uint32_t f = 0; f < facet_count; ++f) {
        const uint8_t* facet = bytes.data() + kBinaryHeaderSize
            + static_cast<size_t>(f) * kBinaryFacetSize;
        // 布局：法向 3×float32（跳过）+ 顶点 9×float32 + 属性计数 uint16（跳过）
        for (size_t c = 0; c < kTriangleCornerCount; ++c) {
            for (size_t axis = 0; axis < 3; ++axis) {
                float coordinate = 0.0f;
                std::memcpy(&coordinate, facet + 12 + (c * 3 + axis) * sizeof(float), sizeof(coordinate));
                corners[c][axis] = static_cast<double>(coordinate);
            }
        }
        weldTriangle(corners.data(), vertex_ids, mesh);
    }

    if (facet_count == 0) {
        spdlog::error("StlModelHandler: binary file declares no facet");
        return false;
    }

    mesh.vertex_count_ = static_cast<Index>(mesh.vertex_positions_.size());
    spdlog::info("StlModelHandler: read {} facets, {} welded vertices (binary)",
        facet_count, mesh.vertex_count_);
    return true;
}

//! @brief 按文件大小与声明三角形数探测：满足二进制布局则走二进制，否则按 ASCII 解析
bool detectBinaryLayout(const std::vector<uint8_t>& bytes)
{
    if (bytes.size() < kBinaryHeaderSize) {
        return false;
    }
    uint32_t facet_count = 0;
    std::memcpy(&facet_count, bytes.data() + 80, sizeof(facet_count));
    return kBinaryHeaderSize + static_cast<size_t>(facet_count) * kBinaryFacetSize == bytes.size();
}

/**
 * @brief 把一个组件的三角形面网格追加到 merged，多组件导出时按点偏移拼成一个网格
 *
 * MeshData 自包含、连通性存组件内局部点索引，追加时统一加 vertex_offset。
 * STL 只承载三角形面，非三角形面与越界脏面告警后跳过。
 * @param vertex_offset 入参为当前文件内点偏移，出参累加本组件的点数
 */
bool appendComponentMesh(const ComponentData& component, MeshData& merged, Index& vertex_offset)
{
    const MeshData* source = component.mesh.get();
    if (!source) {
        return false;
    }

    const Index point_count = static_cast<Index>(source->vertex_positions_.size());
    if (point_count <= 0) {
        spdlog::warn("StlModelHandler: component {} has no vertices, skip", component.id);
        return false;
    }

    merged.vertex_positions_.insert(merged.vertex_positions_.end(),
        source->vertex_positions_.begin(), source->vertex_positions_.end());

    Index skipped_faces = 0;
    if (source->face_vertices_offset_.size() >= 2) {
        const Index face_count = static_cast<Index>(source->face_vertices_offset_.size() - 1);
        const Index corner_count = static_cast<Index>(source->face_vertices_.size());

        for (Index f = 0; f < face_count; ++f) {
            const Index begin = source->face_vertices_offset_[static_cast<size_t>(f)];
            const Index end = source->face_vertices_offset_[static_cast<size_t>(f) + 1];
            // STL 只承载三角形：非三角形与越界脏面整面跳过，不带入导出结果
            if (begin < 0 || end < begin || end > corner_count
                || end - begin != static_cast<Index>(kTriangleCornerCount)) {
                ++skipped_faces;
                continue;
            }

            bool face_ok = true;
            std::array<Index, kTriangleCornerCount> local_corners {};
            for (size_t c = 0; c < kTriangleCornerCount; ++c) {
                const Index point_id = source->face_vertices_[static_cast<size_t>(begin + static_cast<Index>(c))];
                if (point_id < 0 || point_id >= point_count) {
                    face_ok = false;
                    break;
                }
                local_corners[c] = vertex_offset + point_id;
            }
            if (!face_ok) {
                ++skipped_faces;
                continue;
            }

            merged.face_vertices_.insert(merged.face_vertices_.end(),
                local_corners.begin(), local_corners.end());
            merged.face_vertices_offset_.push_back(static_cast<Index>(merged.face_vertices_.size()));
        }
    }

    if (skipped_faces > 0) {
        spdlog::warn("StlModelHandler: component {} has {} non-triangle/dirty face(s), skip",
            component.id, skipped_faces);
    }

    vertex_offset += point_count;
    return true;
}

//! @brief 按角点几何计算单位法向，退化面（零面积叉积）返回 0 向量
std::array<double, 3> faceNormal(const MeshData& mesh, const std::array<Index, 3>& corners)
{
    const auto& p0 = mesh.vertex_positions_[static_cast<size_t>(corners[0])];
    const auto& p1 = mesh.vertex_positions_[static_cast<size_t>(corners[1])];
    const auto& p2 = mesh.vertex_positions_[static_cast<size_t>(corners[2])];
    const std::array<double, 3> u { p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2] };
    const std::array<double, 3> v { p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2] };
    const std::array<double, 3> n { u[1] * v[2] - u[2] * v[1],
        u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0] };
    const double length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (length <= 0.0) {
        return { 0.0, 0.0, 0.0 };
    }
    return { n[0] / length, n[1] / length, n[2] / length };
}

//! @brief 写出 ASCII STL：solid 名固定，facet 法向按顶点几何重算
bool writeAsciiStl(const std::filesystem::path& path, const MeshData& mesh)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        spdlog::error("StlModelHandler: failed to open file '{}' for writing", path.string());
        return false;
    }

    // 17 位有效数字保证 double 往返无损
    output << std::setprecision(17);
    output << "solid PreCess\n";

    const Index face_count = static_cast<Index>(mesh.face_vertices_offset_.size() - 1);
    for (Index f = 0; f < face_count; ++f) {
        const Index begin = mesh.face_vertices_offset_[static_cast<size_t>(f)];
        std::array<Index, kTriangleCornerCount> corners {};
        for (size_t c = 0; c < kTriangleCornerCount; ++c) {
            corners[c] = mesh.face_vertices_[static_cast<size_t>(begin + static_cast<Index>(c))];
        }
        const std::array<double, 3> normal = faceNormal(mesh, corners);
        output << "  facet normal " << normal[0] << ' ' << normal[1] << ' ' << normal[2] << '\n';
        output << "    outer loop\n";
        for (const Index corner : corners) {
            const auto& position = mesh.vertex_positions_[static_cast<size_t>(corner)];
            output << "      vertex " << position[0] << ' ' << position[1] << ' ' << position[2] << '\n';
        }
        output << "    endloop\n";
        output << "  endfacet\n";
    }
    output << "endsolid PreCess\n";

    output.flush();
    if (!output) {
        spdlog::error("StlModelHandler: failed to write file '{}'", path.string());
        return false;
    }

    spdlog::info("StlModelHandler: wrote STL file '{}' ({} vertices, {} facets)",
        path.string(), mesh.vertex_positions_.size(), face_count);
    return true;
}

} // namespace

namespace systems::io {
using core::ArgType;

std::optional<ModelPayload> StlModelHandler::read_model(const fs::path& path, const std::vector<std::any>& args)
{
    // STL 只承载点与三角形面，读入结果是一个网格组件
    auto mesh = std::make_unique<MeshData>();

    try {
        // 二进制读入整个文件：ASCII/二进制两种布局共用一份字节流
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            spdlog::error("StlModelHandler: failed to open file '{}'", path.string());
            return std::nullopt;
        }
        const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>());

        // 先按二进制布局探测（大小与声明数自洽）；不满足再按 ASCII 解析，
        // 兼容部分二进制文件以 "solid" 开头的头部
        bool ok = false;
        if (detectBinaryLayout(bytes)) {
            ok = readBinary(bytes, *mesh);
        } else {
            std::istringstream text;
            text.str(std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
            ok = readAscii(text, *mesh);
        }
        if (!ok) {
            spdlog::error("StlModelHandler: failed to read STL file: {}", path.string());
            return std::nullopt;
        }
    } catch (const std::exception& e) {
        // 整文件字节流拷贝与 ASCII 解析可能触发的 std::length_error / std::bad_alloc
        // 等在此兜底，不让异常逃逸出 IO 边界
        spdlog::error("StlModelHandler: exception reading '{}': {}", path.string(), e.what());
        return std::nullopt;
    }

    auto component = std::make_unique<ComponentData>();
    component->id = -1; // 组件 id 由模型层入池时分配
    component->name = "Comp_0"; // STL 无分组概念，整个文件作为一个组件
    component->mesh = std::move(mesh);

    ComponentDatas components;
    components.push_back(std::move(component));

    return ModelPayload { path.filename().u8string(), std::move(components) };
}

void StlModelHandler::write_components(const ModelLayer& mgr,
    const std::vector<Index>& component_ids,
    const fs::path& path,
    const std::vector<std::any>& /*args*/)
{
    if (component_ids.empty()) {
        spdlog::error("StlModelHandler: write_components called with empty component_ids");
        return;
    }

    MeshData merged;
    merged.init();

    Index vertex_offset = 0;
    int merged_count = 0;
    for (Index cid : component_ids) {
        const ComponentData* component = mgr.findComponent(cid);
        if (!component) {
            spdlog::warn("StlModelHandler: component {} not found, skip", cid);
            continue;
        }
        if (!component->mesh) {
            spdlog::warn("StlModelHandler: component {} has no mesh, skip", cid);
            continue;
        }

        if (appendComponentMesh(*component, merged, vertex_offset)) {
            ++merged_count;
        }
    }

    if (merged_count == 0 || merged.face_vertices_offset_.size() < 2) {
        spdlog::error("StlModelHandler: no triangle mesh component to export");
        return;
    }

    merged.vertex_count_ = static_cast<Index>(merged.vertex_positions_.size());
    if (writeAsciiStl(path, merged)) {
        spdlog::info("StlModelHandler: wrote STL file: {} (components_merged={})", path.string(), merged_count);
    } else {
        spdlog::error("StlModelHandler: failed to write STL file: {}", path.string());
    }
}

std::vector<ArgType> StlModelHandler::read_args_type() const
{
    return {};
}

std::vector<ArgType> StlModelHandler::write_args_type() const
{
    return {};
}
}
