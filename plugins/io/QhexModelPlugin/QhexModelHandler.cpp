/**
 * @file QhexModelHandler.cpp
 * @brief Qhex 六面体网格文件读写实现
 *
 * Qhex 为实验室定义的 ASCII 六面体体网格格式，每行一条记录：
 * - `Vertex <id> x y z`：顶点，id 为 1 基文件内编号，不要求连续
 * - `Hex <id> v1..v8`：六面体体单元，vi 引用顶点 id，顶点顺序为
 *   VTK_HEXAHEDRON 约定（下底面 v1-v4，上底面 v5-v8，右手法则外向）
 */
#include "QhexModelHandler.h"

#include "ArgType.h"
#include "ComponentData.h"
#include "MeshData.h"
#include "ModelData.h"
#include "ModelLayer.h"

#include <spdlog/spdlog.h>

#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace {

//! @brief VTK_HEXAHEDRON 的体单元类型码（MeshData::solid_types_ 采用 VTK 编号）
constexpr unsigned char kVtkHexahedron = 12;
//! @brief 每个六面体单元的顶点数
constexpr size_t kHexCornerCount = 8;

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

//! @brief 把顶点记录追加进网格，登记文件点 id -> 局部点 id 的映射
bool appendVertex(const std::string& line, std::istringstream& record,
    std::unordered_map<Index, Index>& vertex_ids, MeshData& mesh)
{
    Index id = 0;
    std::array<double, 3> position { 0.0, 0.0, 0.0 };
    if (!(record >> id >> position[0] >> position[1] >> position[2])) {
        spdlog::error("QhexModelHandler: bad vertex record '{}'", line);
        return false;
    }
    if (!vertex_ids.emplace(id, static_cast<Index>(mesh.vertex_positions_.size())).second) {
        spdlog::error("QhexModelHandler: duplicate vertex id {}", id);
        return false;
    }
    mesh.vertex_positions_.push_back(position);
    return true;
}

//! @brief 把六面体记录换算为局部点索引后追加进体单元数组
bool appendHex(const std::string& line, std::istringstream& record,
    const std::unordered_map<Index, Index>& vertex_ids, MeshData& mesh)
{
    Index id = 0;
    std::array<Index, kHexCornerCount> corners {};
    if (!(record >> id)) {
        spdlog::error("QhexModelHandler: bad hex record '{}'", line);
        return false;
    }
    // 六面体 id 仅作行序标识不参与建模，行尾附加字段（如属性）照 OFF 惯例丢弃
    for (auto& corner : corners) {
        if (!(record >> corner)) {
            spdlog::error("QhexModelHandler: hex record '{}' has fewer than {} corners", line, kHexCornerCount);
            return false;
        }
    }

    const auto mapped = vertex_ids.find(corners[0]);
    if (mapped == vertex_ids.end()) {
        spdlog::error("QhexModelHandler: hex {} references unknown vertex id {}", id, corners[0]);
        return false;
    }

    mesh.solid_types_.push_back(kVtkHexahedron);
    for (const Index corner : corners) {
        const auto it = vertex_ids.find(corner);
        if (it == vertex_ids.end()) {
            spdlog::error("QhexModelHandler: hex {} references unknown vertex id {}", id, corner);
            return false;
        }
        mesh.solid_vertices_.push_back(it->second);
    }
    mesh.solid_vertices_offset_.push_back(static_cast<Index>(mesh.solid_vertices_.size()));
    mesh.solid_faces_offset_.push_back(0);
    return true;
}

/**
 * @brief 读取 Qhex 主体：逐行解析 Vertex / Hex 记录，未知关键字行告警后跳过
 *
 * 对未知行保持宽容以兼容格式的版本演进；已知关键字的记录损坏（字段缺失、
 * 点 id 重复、六面体引用不存在的顶点 id）则整体读取失败。
 */
bool readQhex(std::istream& input, MeshData& mesh)
{
    mesh.init();

    // 文件点 id（1 基、不要求连续）-> 组件内局部点 id（0 基、按出现顺序）
    std::unordered_map<Index, Index> vertex_ids;
    Index hex_count = 0;
    std::unordered_set<std::string> unknown_keywords;

    std::string raw;
    while (std::getline(input, raw)) {
        const std::string line = trimLine(raw);
        if (line.empty()) {
            continue;
        }

        std::istringstream record(line);
        std::string keyword;
        record >> keyword;
        if (keyword == "Vertex") {
            if (!appendVertex(line, record, vertex_ids, mesh)) {
                return false;
            }
        } else if (keyword == "Hex") {
            if (!appendHex(line, record, vertex_ids, mesh)) {
                return false;
            }
            ++hex_count;
        } else if (unknown_keywords.insert(keyword).second) {
            spdlog::warn("QhexModelHandler: unknown keyword '{}', lines skipped", keyword);
        }
    }

    if (mesh.vertex_positions_.empty() || hex_count == 0) {
        spdlog::error("QhexModelHandler: no mesh records found ({} vertex record(s), {} hex record(s))",
            mesh.vertex_positions_.size(), hex_count);
        return false;
    }

    mesh.vertex_count_ = static_cast<Index>(mesh.vertex_positions_.size());
    spdlog::info("QhexModelHandler: read {} vertices, {} hexahedra",
        mesh.vertex_count_, hex_count);
    return true;
}

/**
 * @brief 把一个组件的六面体网格追加到 merged，多组件导出时按点偏移拼成一个网格
 *
 * MeshData 自包含、连通性存组件内局部点索引，追加时统一加 vertex_offset
 * 换算为文件内点序号（Qhex 没有组件概念，导出结果是一个整体网格）。
 * Qhex 只承载六面体，其余体类型与面/边单元告警后丢弃。
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
        spdlog::warn("QhexModelHandler: component {} has no vertices, skip", component.id);
        return false;
    }

    merged.vertex_positions_.insert(merged.vertex_positions_.end(),
        source->vertex_positions_.begin(), source->vertex_positions_.end());

    Index skipped_solids = 0;
    if (source->solid_vertices_offset_.size() >= 2
        && source->solid_types_.size() + 1 == source->solid_vertices_offset_.size()) {
        const Index solid_count = static_cast<Index>(source->solid_types_.size());
        const Index corner_count = static_cast<Index>(source->solid_vertices_.size());

        for (Index s = 0; s < solid_count; ++s) {
            const Index begin = source->solid_vertices_offset_[static_cast<size_t>(s)];
            const Index end = source->solid_vertices_offset_[static_cast<size_t>(s) + 1];
            // 非 VTK_HEXAHEDRON、角点数不符与越界脏单元整块跳过，不带入导出结果
            if (source->solid_types_[static_cast<size_t>(s)] != kVtkHexahedron
                || end - begin != static_cast<Index>(kHexCornerCount)
                || begin < 0 || end > corner_count) {
                ++skipped_solids;
                continue;
            }

            // 先逐点校验通过后再追加，避免脏单元污染已合并的数据
            bool solid_ok = true;
            std::array<Index, kHexCornerCount> local_corners {};
            for (size_t c = 0; c < kHexCornerCount; ++c) {
                const Index point_id = source->solid_vertices_[static_cast<size_t>(begin + static_cast<Index>(c))];
                if (point_id < 0 || point_id >= point_count) {
                    solid_ok = false;
                    break;
                }
                local_corners[c] = vertex_offset + point_id;
            }
            if (!solid_ok) {
                ++skipped_solids;
                continue;
            }

            merged.solid_types_.push_back(kVtkHexahedron);
            merged.solid_vertices_.insert(merged.solid_vertices_.end(),
                local_corners.begin(), local_corners.end());
            merged.solid_vertices_offset_.push_back(static_cast<Index>(merged.solid_vertices_.size()));
        }
    }

    if (skipped_solids > 0) {
        spdlog::warn("QhexModelHandler: component {} has {} non-hex/dirty solid(s), skip", component.id, skipped_solids);
    }

    vertex_offset += point_count;
    return true;
}

//! @brief 写出合并后的网格：先顶点再六面体，文件内 id 均为 1 基顺序编号
bool writeQhex(const std::filesystem::path& path, const MeshData& mesh)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        spdlog::error("QhexModelHandler: failed to open file '{}' for writing", path.string());
        return false;
    }

    // 17 位有效数字保证 double 往返无损
    output << std::setprecision(17);
    const Index vertex_count = static_cast<Index>(mesh.vertex_positions_.size());
    for (Index v = 0; v < vertex_count; ++v) {
        const auto& position = mesh.vertex_positions_[static_cast<size_t>(v)];
        output << "Vertex " << v + 1 << ' ' << position[0] << ' ' << position[1] << ' ' << position[2] << '\n';
    }

    const Index solid_count = static_cast<Index>(mesh.solid_types_.size());
    for (Index s = 0; s < solid_count; ++s) {
        const Index begin = mesh.solid_vertices_offset_[static_cast<size_t>(s)];
        const Index end = mesh.solid_vertices_offset_[static_cast<size_t>(s) + 1];
        output << "Hex " << s + 1;
        for (Index c = begin; c < end; ++c) {
            output << ' ' << mesh.solid_vertices_[static_cast<size_t>(c)] + 1;
        }
        output << '\n';
    }

    output.flush();
    if (!output) {
        spdlog::error("QhexModelHandler: failed to write file '{}'", path.string());
        return false;
    }

    spdlog::info("QhexModelHandler: wrote Qhex file '{}' ({} vertices, {} hexahedra)",
        path.string(), vertex_count, solid_count);
    return true;
}

} // namespace

namespace systems::io {
using core::ArgType;

std::optional<ModelPayload> QhexModelHandler::read_model(const fs::path& path, const std::vector<std::any>& args)
{
    // Qhex 只承载点与六面体，读入结果是一个网格组件
    auto mesh = std::make_unique<MeshData>();

    // 二进制打开：行尾 '\r' 由 trimLine 处理
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        spdlog::error("QhexModelHandler: failed to open file '{}'", path.string());
        return std::nullopt;
    }
    if (!readQhex(input, *mesh)) {
        spdlog::error("QhexModelHandler: failed to read Qhex file: {}", path.string());
        return std::nullopt;
    }

    auto component = std::make_unique<ComponentData>();
    component->id = -1; // 组件 id 由模型层入池时分配
    component->name = "Comp_0"; // Qhex 无分组概念，整个文件作为一个组件
    component->mesh = std::move(mesh);

    ComponentDatas components;
    components.push_back(std::move(component));

    return ModelPayload { path.filename().u8string(), std::move(components) };
}

void QhexModelHandler::write_components(const ModelLayer& mgr,
    const std::vector<Index>& component_ids,
    const fs::path& path,
    const std::vector<std::any>& /*args*/)
{
    if (component_ids.empty()) {
        spdlog::error("QhexModelHandler: write_components called with empty component_ids");
        return;
    }

    MeshData merged;
    merged.init(); // 体 offsets 至少有 {0}

    Index vertex_offset = 0;
    int merged_count = 0;
    for (Index cid : component_ids) {
        const ComponentData* component = mgr.findComponent(cid);
        if (!component) {
            spdlog::warn("QhexModelHandler: component {} not found, skip", cid);
            continue;
        }
        if (!component->mesh) {
            spdlog::warn("QhexModelHandler: component {} has no mesh, skip", cid);
            continue;
        }

        if (appendComponentMesh(*component, merged, vertex_offset)) {
            ++merged_count;
        }
    }

    if (merged_count == 0 || merged.solid_types_.empty()) {
        spdlog::error("QhexModelHandler: no hexahedral mesh component to export");
        return;
    }

    merged.vertex_count_ = static_cast<Index>(merged.vertex_positions_.size());
    if (writeQhex(path, merged)) {
        spdlog::info("QhexModelHandler: wrote Qhex file: {} (components_merged={})", path.string(), merged_count);
    } else {
        spdlog::error("QhexModelHandler: failed to write Qhex file: {}", path.string());
    }
}

std::vector<ArgType> QhexModelHandler::read_args_type() const
{
    return {};
}

std::vector<ArgType> QhexModelHandler::write_args_type() const
{
    return {};
}
}
