/**
 * @file GmshModelHandler.cpp
 * @brief Gmsh 网格文件读写实现
 *
 * 支持 ASCII 的 2.2（历史交换版本）与 4.1（Gmsh 4.x 默认）两种 msh 版本：
 * - 2.2：$Nodes 逐点 `tag x y z`；$Elements 逐单元
 *   `elm-tag elm-type num-tags <tags...> node-ids...`；
 * - 4.1：$Nodes / $Elements 按 entity block 组织，tag 与坐标分两段存储，
 *   单元行不含 tags；
 * - 二进制（file-type = 1）与参数化节点（parametric = 1）不支持，读入失败。
 *
 * 单元类型映射（gmsh elm type -> 项目 MeshData 单元类别，体类型取 VTK 编号）：
 * 1 线段->边单元，2/3 三角形/四边形->面单元，4/5/6/7 四面体/六面体/
 * 三棱柱/金字塔->体单元，15 点->跳过；高阶与未知类型统计后告警跳过。
 * 写出固定 2.2 ASCII（tags 写 2 个占位 0）。
 */
#include "GmshModelHandler.h"

#include "ArgType.h"
#include "ComponentData.h"
#include "MeshData.h"
#include "ModelData.h"
#include "ModelLayer.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

//! @brief VTK 体单元类型码（MeshData::solid_types_ 采用 VTK 编号）
constexpr unsigned char kVtkTetra = 10;
constexpr unsigned char kVtkHexahedron = 12;
constexpr unsigned char kVtkWedge = 13;
constexpr unsigned char kVtkPyramid = 14;

//! @brief 读入的单元类别
enum class ElementCategory {
    Skip, //> 不产生建模数据（点单元等）
    Edge, //> 边单元
    Face, //> 面单元
    Solid, //> 体单元
};

//! @brief gmsh 单元类型的读入描述：角点数、类别与体单元 VTK 类型码
struct ElementTypeSpec {
    size_t corner_count;
    ElementCategory category;
    unsigned char vtk_type; //> 仅 Solid 类别有效
};

//! @brief gmsh 一阶单元类型表；不在表中的类型（高阶/未知）按 Skip 统计告警
const std::map<int, ElementTypeSpec>& elementTypeTable()
{
    static const std::map<int, ElementTypeSpec> table {
        { 1, { 2, ElementCategory::Edge, 0 } },
        { 2, { 3, ElementCategory::Face, 0 } },
        { 3, { 4, ElementCategory::Face, 0 } },
        { 4, { 4, ElementCategory::Solid, kVtkTetra } },
        { 5, { 8, ElementCategory::Solid, kVtkHexahedron } },
        { 6, { 6, ElementCategory::Solid, kVtkWedge } },
        { 7, { 5, ElementCategory::Solid, kVtkPyramid } },
        { 15, { 1, ElementCategory::Skip, 0 } },
    };
    return table;
}

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

//! @brief 读取一行并要求其非空（节内的计数行、数据行不允许为空）
bool nextContentLine(std::istream& input, std::string& line)
{
    while (std::getline(input, line)) {
        line = trimLine(line);
        if (!line.empty()) {
            return true;
        }
    }
    return false;
}

/**
 * @brief 跳过未识别的节：逐行读到 `$End<name>` 为止
 *
 * 对未识别节保持宽容以兼容格式的版本演进（$Entities、$Periodic 等）。
 */
void skipSection(std::istream& input, const std::string& section)
{
    const std::string end_marker = "$End" + section.substr(1);
    std::string line;
    while (std::getline(input, line)) {
        if (trimLine(line) == end_marker) {
            return;
        }
    }
    spdlog::warn("GmshModelHandler: section {} truncated (missing {})", section, end_marker);
}

//! @brief msh 版本与布局信息
struct MeshFormat {
    double version { 0.0 };
    int file_type { 0 }; //> 0 = ASCII，1 = 二进制
};

//! @brief 解析 $MeshFormat 节，要求支持范围内（2.2/4.1、ASCII）
bool readMeshFormat(std::istream& input, MeshFormat& format)
{
    std::string line;
    if (!nextContentLine(input, line)) {
        spdlog::error("GmshModelHandler: $MeshFormat has no payload");
        return false;
    }
    int data_size = 0;
    std::istringstream record(line);
    if (!(record >> format.version >> format.file_type >> data_size)) {
        spdlog::error("GmshModelHandler: bad $MeshFormat line '{}'", line);
        return false;
    }
    if (format.file_type != 0) {
        spdlog::error("GmshModelHandler: binary msh (file-type {}) is not supported, export as ASCII",
            format.file_type);
        return false;
    }
    if (format.version >= 2.0 && format.version < 3.0) {
        return true; // 2.0/2.1/2.2 的 $Nodes/$Elements 布局一致，统一按 2.2 布局解析
    }
    if (format.version >= 4.0 && format.version < 4.2) {
        return true; // 4.1 及相近的 4.x
    }
    spdlog::error("GmshModelHandler: unsupported msh version {}", format.version);
    return false;
}

/**
 * @brief Gmsh 单元的读入装配目标：按类别分发进 MeshData 的三个连通性数组
 *
 * 文件点 tag 经 node_ids 换算为组件内局部点 id；角点数与类别的合法性
 * 已由类型表保证，这里只负责装配与引用校验。
 */
class ElementAssembler {
public:
    explicit ElementAssembler(const std::unordered_map<Index, Index>& node_ids)
        : node_ids_(node_ids)
    {
    }

    //! @brief 追加一个单元；返回 false 表示引用了未知点 tag，整体读取失败
    bool append(int gmsh_type, const std::vector<Index>& file_tags, MeshData& mesh)
    {
        const auto it = elementTypeTable().find(gmsh_type);
        if (it == elementTypeTable().end() || it->second.category == ElementCategory::Skip) {
            ++skipped_types_[gmsh_type];
            return true;
        }
        const ElementTypeSpec& spec = it->second;
        if (file_tags.size() != spec.corner_count) {
            spdlog::error("GmshModelHandler: element type {} expects {} nodes, got {}",
                gmsh_type, spec.corner_count, file_tags.size());
            return false;
        }

        // 先整体换算局部点 id，校验通过后再装配，避免脏单元污染已装配数据
        std::vector<Index> local_ids;
        local_ids.reserve(file_tags.size());
        for (const Index tag : file_tags) {
            const auto mapped = node_ids_.find(tag);
            if (mapped == node_ids_.end()) {
                spdlog::error("GmshModelHandler: element references unknown node tag {}", tag);
                return false;
            }
            local_ids.push_back(mapped->second);
        }

        // Skip 类别已在入口提前返回，走到这里即实际装配
        ++assembled_count_;
        switch (spec.category) {
        case ElementCategory::Edge:
            mesh.edge_vertices_.insert(mesh.edge_vertices_.end(),
                local_ids.begin(), local_ids.end());
            break;
        case ElementCategory::Face:
            mesh.face_vertices_.insert(mesh.face_vertices_.end(),
                local_ids.begin(), local_ids.end());
            mesh.face_vertices_offset_.push_back(
                static_cast<Index>(mesh.face_vertices_.size()));
            break;
        case ElementCategory::Solid:
            mesh.solid_types_.push_back(spec.vtk_type);
            mesh.solid_vertices_.insert(mesh.solid_vertices_.end(),
                local_ids.begin(), local_ids.end());
            mesh.solid_vertices_offset_.push_back(
                static_cast<Index>(mesh.solid_vertices_.size()));
            mesh.solid_faces_offset_.push_back(0);
            break;
        case ElementCategory::Skip:
            break;
        }
        return true;
    }

    //! @brief 已实际装配进网格的单元数（点/高阶/未知等跳过单元不计入）
    size_t assembledCount() const { return assembled_count_; }

    //! @brief 汇总输出跳过类型的告警（每类型一次）
    void reportSkipped()
    {
        for (const auto& [type, count] : skipped_types_) {
            spdlog::warn("GmshModelHandler: {} element(s) of type {} skipped", count, type);
        }
        skipped_types_.clear();
    }

private:
    const std::unordered_map<Index, Index>& node_ids_;
    std::map<int, size_t> skipped_types_;
    size_t assembled_count_ = 0;
};

/**
 * @brief 校验声明计数不超过按文件大小推得的上限
 *
 * 每条记录在文件中至少占 2 字节（最短 token + 换行），声明计数超过
 * 文件大小的一半即必然非法；先行拒绝避免畸形文件的天文数字触发
 * std::length_error / std::bad_alloc 逃逸出读取边界。
 */
bool countWithinLimit(long long count, long long max_count, const std::string& section)
{
    if (count <= max_count) {
        return true;
    }
    spdlog::error("GmshModelHandler: {} count {} exceeds file size limit {}", section, count, max_count);
    return false;
}

//! @brief 解析 2.2 版 $Nodes：N 行 `tag x y z`，tag 任意、按出现顺序建局部映射
bool readNodes2_2(std::istream& input, std::unordered_map<Index, Index>& node_ids, MeshData& mesh,
    long long max_count)
{
    std::string line;
    if (!nextContentLine(input, line)) {
        return false;
    }
    long long declared_count = 0;
    std::istringstream count_record(line);
    if (!(count_record >> declared_count) || declared_count < 0
        || !countWithinLimit(declared_count, max_count, "$Nodes")) {
        spdlog::error("GmshModelHandler: bad $Nodes count line '{}'", line);
        return false;
    }

    for (long long n = 0; n < declared_count; ++n) {
        if (!nextContentLine(input, line)) {
            spdlog::error("GmshModelHandler: $Nodes truncated at {}/{}", n, declared_count);
            return false;
        }
        Index tag = 0;
        std::array<double, 3> position { 0.0, 0.0, 0.0 };
        std::istringstream record(line);
        if (!(record >> tag >> position[0] >> position[1] >> position[2])) {
            spdlog::error("GmshModelHandler: bad node record '{}'", line);
            return false;
        }
        if (!node_ids.emplace(tag, static_cast<Index>(mesh.vertex_positions_.size())).second) {
            spdlog::error("GmshModelHandler: duplicate node tag {}", tag);
            return false;
        }
        mesh.vertex_positions_.push_back(position);
    }
    return true;
}

//! @brief 解析 4.1 版 $Nodes：按 entity block，tag 段与坐标段分离存储
bool readNodes4_1(std::istream& input, std::unordered_map<Index, Index>& node_ids, MeshData& mesh,
    long long max_count)
{
    std::string line;
    if (!nextContentLine(input, line)) {
        return false;
    }
    long long block_count = 0;
    long long declared_nodes = 0;
    std::istringstream head(line);
    long long min_tag = 0, max_tag = 0;
    if (!(head >> block_count >> declared_nodes >> min_tag >> max_tag) || block_count < 0
        || !countWithinLimit(declared_nodes, max_count, "$Nodes")
        || !countWithinLimit(block_count, max_count, "$Nodes blocks")) {
        spdlog::error("GmshModelHandler: bad $Nodes header '{}'", line);
        return false;
    }

    for (long long b = 0; b < block_count; ++b) {
        if (!nextContentLine(input, line)) {
            spdlog::error("GmshModelHandler: $Nodes truncated at block {}/{}", b, block_count);
            return false;
        }
        int entity_dim = 0, entity_tag = 0, parametric = 0;
        long long nodes_in_block = 0;
        std::istringstream record(line);
        if (!(record >> entity_dim >> entity_tag >> parametric >> nodes_in_block)
            || nodes_in_block < 0
            || !countWithinLimit(nodes_in_block, max_count, "$Nodes block entries")) {
            spdlog::error("GmshModelHandler: bad $Nodes block header '{}'", line);
            return false;
        }
        if (parametric != 0) {
            spdlog::error("GmshModelHandler: parametric nodes are not supported");
            return false;
        }

        // 先读 tags 段再读坐标段，按 tag 建立局部映射
        std::vector<Index> block_tags(static_cast<size_t>(nodes_in_block));
        for (auto& tag : block_tags) {
            if (!nextContentLine(input, line)) {
                spdlog::error("GmshModelHandler: $Nodes tag section truncated");
                return false;
            }
            std::istringstream tag_record(line);
            if (!(tag_record >> tag)) {
                spdlog::error("GmshModelHandler: bad node tag line '{}'", line);
                return false;
            }
        }
        for (const Index tag : block_tags) {
            if (!nextContentLine(input, line)) {
                spdlog::error("GmshModelHandler: $Nodes coordinate section truncated");
                return false;
            }
            std::array<double, 3> position { 0.0, 0.0, 0.0 };
            std::istringstream coord_record(line);
            if (!(coord_record >> position[0] >> position[1] >> position[2])) {
                spdlog::error("GmshModelHandler: bad node coordinate line '{}'", line);
                return false;
            }
            if (!node_ids.emplace(tag, static_cast<Index>(mesh.vertex_positions_.size())).second) {
                spdlog::error("GmshModelHandler: duplicate node tag {}", tag);
                return false;
            }
            mesh.vertex_positions_.push_back(position);
        }
    }
    return true;
}

//! @brief 解析 2.2 版 $Elements：`elm-tag elm-type num-tags <tags> node-ids`
bool readElements2_2(std::istream& input, ElementAssembler& assembler, MeshData& mesh,
    long long max_count)
{
    std::string line;
    if (!nextContentLine(input, line)) {
        return false;
    }
    long long declared_count = 0;
    std::istringstream count_record(line);
    if (!(count_record >> declared_count) || declared_count < 0
        || !countWithinLimit(declared_count, max_count, "$Elements")) {
        spdlog::error("GmshModelHandler: bad $Elements count line '{}'", line);
        return false;
    }

    for (long long e = 0; e < declared_count; ++e) {
        if (!nextContentLine(input, line)) {
            spdlog::error("GmshModelHandler: $Elements truncated at {}/{}", e, declared_count);
            return false;
        }
        std::istringstream record(line);
        Index element_tag = 0;
        int gmsh_type = 0, num_tags = 0;
        if (!(record >> element_tag >> gmsh_type >> num_tags) || num_tags < 0) {
            spdlog::error("GmshModelHandler: bad element record '{}'", line);
            return false;
        }
        // 单元 tag 不参与建模；任意数量的属性 tags 跳过后读角点
        bool tags_ok = true;
        for (int t = 0; t < num_tags; ++t) {
            Index ignored = 0;
            if (!(record >> ignored)) {
                tags_ok = false;
                break;
            }
        }
        if (!tags_ok) {
            spdlog::error("GmshModelHandler: element record '{}' has fewer than {} tags",
                line, num_tags);
            return false;
        }

        const auto it = elementTypeTable().find(gmsh_type);
        const size_t corner_count = it != elementTypeTable().end() ? it->second.corner_count : 0;
        std::vector<Index> corners(corner_count);
        for (auto& corner : corners) {
            if (!(record >> corner)) {
                spdlog::error("GmshModelHandler: element record '{}' has fewer than {} nodes",
                    line, corner_count);
                return false;
            }
        }
        if (!assembler.append(gmsh_type, corners, mesh)) {
            return false;
        }
    }
    assembler.reportSkipped();
    return true;
}

//! @brief 解析 4.1 版 $Elements：按 entity block，单元行仅 `elem-tag node-ids`
bool readElements4_1(std::istream& input, ElementAssembler& assembler, MeshData& mesh,
    long long max_count)
{
    std::string line;
    if (!nextContentLine(input, line)) {
        return false;
    }
    long long block_count = 0;
    std::istringstream head(line);
    long long declared_elements = 0, min_tag = 0, max_tag = 0;
    if (!(head >> block_count >> declared_elements >> min_tag >> max_tag) || block_count < 0
        || !countWithinLimit(declared_elements, max_count, "$Elements")
        || !countWithinLimit(block_count, max_count, "$Elements blocks")) {
        spdlog::error("GmshModelHandler: bad $Elements header '{}'", line);
        return false;
    }

    for (long long b = 0; b < block_count; ++b) {
        if (!nextContentLine(input, line)) {
            spdlog::error("GmshModelHandler: $Elements truncated at block {}/{}", b, block_count);
            return false;
        }
        int entity_dim = 0, entity_tag = 0, element_type = 0;
        long long elements_in_block = 0;
        std::istringstream record(line);
        if (!(record >> entity_dim >> entity_tag >> element_type >> elements_in_block)
            || elements_in_block < 0
            || !countWithinLimit(elements_in_block, max_count, "$Elements block entries")) {
            spdlog::error("GmshModelHandler: bad $Elements block header '{}'", line);
            return false;
        }

        const auto it = elementTypeTable().find(element_type);
        const size_t corner_count = it != elementTypeTable().end() ? it->second.corner_count : 0;
        for (long long e = 0; e < elements_in_block; ++e) {
            if (!nextContentLine(input, line)) {
                spdlog::error("GmshModelHandler: $Elements truncated in block {}", b);
                return false;
            }
            std::istringstream element_record(line);
            Index element_tag = 0;
            if (!(element_record >> element_tag)) {
                spdlog::error("GmshModelHandler: bad element record '{}'", line);
                return false;
            }
            std::vector<Index> corners(corner_count);
            for (auto& corner : corners) {
                if (!(element_record >> corner)) {
                    spdlog::error("GmshModelHandler: element record '{}' has fewer than {} nodes",
                        line, corner_count);
                    return false;
                }
            }
            if (!assembler.append(element_type, corners, mesh)) {
                return false;
            }
        }
    }
    assembler.reportSkipped();
    return true;
}

/**
 * @brief 读取 msh 主体：按节名分发，未知节告警跳过
 *
 * 已知节的损坏（计数非法、记录字段缺失、引用未知点 tag）整体读取失败；
 * 高阶/未知单元类型统计后告警跳过，不阻塞其余单元。
 * @param max_count 声明计数的可信上限（按文件大小推得），防畸形文件巨额分配
 */
bool readGmsh(std::istream& input, MeshData& mesh, long long max_count)
{
    mesh.init();

    MeshFormat format;
    std::unordered_map<Index, Index> node_ids;
    ElementAssembler assembler(node_ids);
    bool nodes_seen = false;
    bool elements_seen = false;
    std::unordered_set<std::string> skipped_sections;

    std::string raw;
    while (std::getline(input, raw)) {
        const std::string line = trimLine(raw);
        if (line.empty()) {
            continue;
        }
        if (line.front() != '$') {
            spdlog::error("GmshModelHandler: stray line outside section '{}'", line);
            return false;
        }
        if (line.rfind("$End", 0) == 0) {
            continue; // 节尾标记由各节的计数控制消费，这里统一放行
        }

        if (line == "$MeshFormat") {
            if (!readMeshFormat(input, format)) {
                return false;
            }
        } else if (line == "$Nodes") {
            if (format.version < 4.0) {
                if (!readNodes2_2(input, node_ids, mesh, max_count)) {
                    return false;
                }
            } else {
                if (!readNodes4_1(input, node_ids, mesh, max_count)) {
                    return false;
                }
            }
            nodes_seen = true;
        } else if (line == "$Elements") {
            if (!nodes_seen) {
                spdlog::error("GmshModelHandler: $Elements appears before $Nodes");
                return false;
            }
            if (format.version < 4.0) {
                if (!readElements2_2(input, assembler, mesh, max_count)) {
                    return false;
                }
            } else {
                if (!readElements4_1(input, assembler, mesh, max_count)) {
                    return false;
                }
            }
            elements_seen = true;
        } else {
            // 告警按节名去重，跳段每次都要执行（同名节可能连续多段，如 $NodeData）
            if (skipped_sections.insert(line).second) {
                spdlog::warn("GmshModelHandler: unknown section {}, skipped", line);
            }
            skipSection(input, line);
        }
    }

    if (!nodes_seen || mesh.vertex_positions_.empty()) {
        spdlog::error("GmshModelHandler: no node records found");
        return false;
    }
    // 与写出侧 has_cells 口径对齐：跳过的点/高阶/未知单元不计入，
    // 只含这类单元的文件整体失败，而非产出无连通性的空网格
    const size_t element_count = assembler.assembledCount();
    if (!elements_seen || element_count == 0) {
        spdlog::error("GmshModelHandler: no supported element records found");
        return false;
    }

    mesh.vertex_count_ = static_cast<Index>(mesh.vertex_positions_.size());
    spdlog::info("GmshModelHandler: read msh {} ({} nodes, {} elements)",
        format.version, mesh.vertex_count_, element_count);
    return true;
}

//! @brief 体单元 VTK 类型码 -> gmsh 类型码（写出反映射）
int gmshSolidType(unsigned char vtk_type)
{
    switch (vtk_type) {
    case kVtkTetra:
        return 4;
    case kVtkHexahedron:
        return 5;
    case kVtkWedge:
        return 6;
    case kVtkPyramid:
        return 7;
    default:
        return -1;
    }
}

/**
 * @brief 把一个组件的网格追加到 merged，多组件导出时按点偏移拼成一个网格
 *
 * MeshData 自包含、连通性存组件内局部点索引，追加时统一加 vertex_offset。
 * msh 只承载一阶边/面/体单元，超纲单元（多边形面、未知体类型）告警跳过。
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
        spdlog::warn("GmshModelHandler: component {} has no vertices, skip", component.id);
        return false;
    }

    merged.vertex_positions_.insert(merged.vertex_positions_.end(),
        source->vertex_positions_.begin(), source->vertex_positions_.end());

    // 边单元：每 2 点一段；先局部收集校验，出现脏边则整组丢弃
    if (source->edge_vertices_.size() % 2 == 0) {
        std::vector<Index> local_edges;
        local_edges.reserve(source->edge_vertices_.size());
        bool edges_ok = true;
        for (const Index point_id : source->edge_vertices_) {
            if (point_id < 0 || point_id >= point_count) {
                edges_ok = false;
                break;
            }
            local_edges.push_back(vertex_offset + point_id);
        }
        if (edges_ok) {
            merged.edge_vertices_.insert(merged.edge_vertices_.end(),
                local_edges.begin(), local_edges.end());
        } else {
            spdlog::warn("GmshModelHandler: component {} has dirty edge, all edges skipped",
                component.id);
        }
    }

    // 面单元：仅三角形/四边形（gmsh type 2/3）
    Index skipped_faces = 0;
    if (source->face_vertices_offset_.size() >= 2) {
        const Index face_count = static_cast<Index>(source->face_vertices_offset_.size() - 1);
        const Index corner_count = static_cast<Index>(source->face_vertices_.size());
        for (Index f = 0; f < face_count; ++f) {
            const Index begin = source->face_vertices_offset_[static_cast<size_t>(f)];
            const Index end = source->face_vertices_offset_[static_cast<size_t>(f) + 1];
            const Index size = end - begin;
            if (begin < 0 || end < begin || end > corner_count || (size != 3 && size != 4)) {
                ++skipped_faces;
                continue;
            }
            bool face_ok = true;
            std::array<Index, 4> corners {};
            for (Index c = begin; c < end; ++c) {
                const Index point_id = source->face_vertices_[static_cast<size_t>(c)];
                if (point_id < 0 || point_id >= point_count) {
                    face_ok = false;
                    break;
                }
                corners[static_cast<size_t>(c - begin)] = vertex_offset + point_id;
            }
            if (!face_ok) {
                ++skipped_faces;
                continue;
            }
            merged.face_vertices_.insert(merged.face_vertices_.end(),
                corners.begin(), corners.begin() + static_cast<ptrdiff_t>(size));
            merged.face_vertices_offset_.push_back(
                static_cast<Index>(merged.face_vertices_.size()));
        }
    }
    if (skipped_faces > 0) {
        spdlog::warn("GmshModelHandler: component {} has {} non-tri/quad face(s), skip",
            component.id, skipped_faces);
    }

    // 体单元：VTK 类型码反查 gmsh 类型码
    Index skipped_solids = 0;
    if (source->solid_vertices_offset_.size() >= 2
        && source->solid_types_.size() + 1 == source->solid_vertices_offset_.size()) {
        const Index solid_count = static_cast<Index>(source->solid_types_.size());
        const Index corner_count = static_cast<Index>(source->solid_vertices_.size());
        for (Index s = 0; s < solid_count; ++s) {
            const Index begin = source->solid_vertices_offset_[static_cast<size_t>(s)];
            const Index end = source->solid_vertices_offset_[static_cast<size_t>(s) + 1];
            const int gmsh_type = gmshSolidType(source->solid_types_[static_cast<size_t>(s)]);
            const auto it = elementTypeTable().find(gmsh_type);
            if (gmsh_type < 0 || it == elementTypeTable().end()
                || begin < 0 || end > corner_count
                || end - begin != static_cast<Index>(it->second.corner_count)) {
                ++skipped_solids;
                continue;
            }
            bool solid_ok = true;
            std::vector<Index> corners(static_cast<size_t>(end - begin));
            for (Index c = begin; c < end; ++c) {
                const Index point_id = source->solid_vertices_[static_cast<size_t>(c)];
                if (point_id < 0 || point_id >= point_count) {
                    solid_ok = false;
                    break;
                }
                corners[static_cast<size_t>(c - begin)] = vertex_offset + point_id;
            }
            if (!solid_ok) {
                ++skipped_solids;
                continue;
            }
            merged.solid_types_.push_back(source->solid_types_[static_cast<size_t>(s)]);
            merged.solid_vertices_.insert(merged.solid_vertices_.end(),
                corners.begin(), corners.end());
            merged.solid_vertices_offset_.push_back(
                static_cast<Index>(merged.solid_vertices_.size()));
            merged.solid_faces_offset_.push_back(0);
        }
    }
    if (skipped_solids > 0) {
        spdlog::warn("GmshModelHandler: component {} has {} unsupported solid(s), skip",
            component.id, skipped_solids);
    }

    vertex_offset += point_count;
    return true;
}

//! @brief 写出 msh 2.2 ASCII：固定 2 个占位 tags（physical/elementary 均为 0）
bool writeMsh2_2(const std::filesystem::path& path, const MeshData& mesh)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        spdlog::error("GmshModelHandler: failed to open file '{}' for writing", path.string());
        return false;
    }
    output << std::setprecision(17);

    output << "$MeshFormat\n2.2 0 8\n$EndMeshFormat\n";

    const Index vertex_count = static_cast<Index>(mesh.vertex_positions_.size());
    output << "$Nodes\n"
           << vertex_count << '\n';
    for (Index v = 0; v < vertex_count; ++v) {
        const auto& position = mesh.vertex_positions_[static_cast<size_t>(v)];
        output << v + 1 << ' ' << position[0] << ' ' << position[1] << ' ' << position[2] << '\n';
    }
    output << "$EndNodes\n";

    // 先统计可写出的单元数（与合并阶段的过滤口径一致）
    const Index face_count = mesh.face_vertices_offset_.size() >= 2
        ? static_cast<Index>(mesh.face_vertices_offset_.size() - 1)
        : 0;
    const Index solid_count = static_cast<Index>(mesh.solid_types_.size());
    const Index edge_count = mesh.edge_vertices_.size() / 2;
    output << "$Elements\n"
           << edge_count + face_count + solid_count << '\n';

    Index element_id = 0;
    for (Index e = 0; e < edge_count; ++e) {
        output << ++element_id << " 1 2 0 0"
               << ' ' << mesh.edge_vertices_[static_cast<size_t>(e * 2)] + 1
               << ' ' << mesh.edge_vertices_[static_cast<size_t>(e * 2 + 1)] + 1 << '\n';
    }
    for (Index f = 0; f < face_count; ++f) {
        const Index begin = mesh.face_vertices_offset_[static_cast<size_t>(f)];
        const Index end = mesh.face_vertices_offset_[static_cast<size_t>(f) + 1];
        // gmsh 面类型码：三角形 2、四边形 3（合并阶段已过滤到只剩这两种）
        output << ++element_id << ' ' << (end - begin == 3 ? 2 : 3) << " 2 0 0";
        for (Index c = begin; c < end; ++c) {
            output << ' ' << mesh.face_vertices_[static_cast<size_t>(c)] + 1;
        }
        output << '\n';
    }
    for (Index s = 0; s < solid_count; ++s) {
        const Index begin = mesh.solid_vertices_offset_[static_cast<size_t>(s)];
        const Index end = mesh.solid_vertices_offset_[static_cast<size_t>(s) + 1];
        output << ++element_id << ' ' << gmshSolidType(mesh.solid_types_[static_cast<size_t>(s)])
               << " 2 0 0";
        for (Index c = begin; c < end; ++c) {
            output << ' ' << mesh.solid_vertices_[static_cast<size_t>(c)] + 1;
        }
        output << '\n';
    }
    output << "$EndElements\n";

    output.flush();
    if (!output) {
        spdlog::error("GmshModelHandler: failed to write file '{}'", path.string());
        return false;
    }

    spdlog::info("GmshModelHandler: wrote msh file '{}' ({} nodes, {} elements)",
        path.string(), vertex_count, element_id);
    return true;
}

} // namespace

namespace systems::io {
using core::ArgType;

std::optional<ModelPayload> GmshModelHandler::read_model(const fs::path& path, const std::vector<std::any>& args)
{
    // msh 承载点与一阶边/面/体单元，读入结果是一个网格组件
    auto mesh = std::make_unique<MeshData>();

    try {
        // 二进制打开：行尾 '\r' 由 trimLine 处理
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            spdlog::error("GmshModelHandler: failed to open file '{}'", path.string());
            return std::nullopt;
        }

        // 声明计数上限按文件大小推得（每条记录至少 2 字节）；文件大小不可得
        // （size_error 置位）时不设上限，退化为仅靠读取循环的截断检查兜底。
        // 只查返回值不可靠：失败时 file_size 返回 uintmax_t(-1) 而非 0
        std::error_code size_error;
        const uintmax_t file_size = fs::file_size(path, size_error);
        const long long max_count = size_error
            ? std::numeric_limits<long long>::max()
            : static_cast<long long>(std::min<uintmax_t>(file_size,
                  static_cast<uintmax_t>(std::numeric_limits<long long>::max())))
                / 2;

        if (!readGmsh(input, *mesh, max_count)) {
            spdlog::error("GmshModelHandler: failed to read msh file: {}", path.string());
            return std::nullopt;
        }
    } catch (const std::exception& e) {
        // 畸形输入可能触发的 std::length_error / std::bad_alloc 等在此兜底，
        // 不让异常逃逸出 IO 边界终止宿主进程
        spdlog::error("GmshModelHandler: exception reading '{}': {}", path.string(), e.what());
        return std::nullopt;
    }

    auto component = std::make_unique<ComponentData>();
    component->id = -1; // 组件 id 由模型层入池时分配
    component->name = "Comp_0"; // v1 不解析 physical 分组，整个文件作为一个组件
    component->mesh = std::move(mesh);

    ComponentDatas components;
    components.push_back(std::move(component));

    return ModelPayload { path.filename().u8string(), std::move(components) };
}

void GmshModelHandler::write_components(const ModelLayer& mgr,
    const std::vector<Index>& component_ids,
    const fs::path& path,
    const std::vector<std::any>& /*args*/)
{
    if (component_ids.empty()) {
        spdlog::error("GmshModelHandler: write_components called with empty component_ids");
        return;
    }

    MeshData merged;
    merged.init();

    Index vertex_offset = 0;
    int merged_count = 0;
    for (Index cid : component_ids) {
        const ComponentData* component = mgr.findComponent(cid);
        if (!component) {
            spdlog::warn("GmshModelHandler: component {} not found, skip", cid);
            continue;
        }
        if (!component->mesh) {
            spdlog::warn("GmshModelHandler: component {} has no mesh, skip", cid);
            continue;
        }

        if (appendComponentMesh(*component, merged, vertex_offset)) {
            ++merged_count;
        }
    }

    const bool has_cells = !merged.edge_vertices_.empty()
        || merged.face_vertices_offset_.size() >= 2
        || !merged.solid_types_.empty();
    if (merged_count == 0 || !has_cells) {
        spdlog::error("GmshModelHandler: no supported mesh component to export");
        return;
    }

    merged.vertex_count_ = static_cast<Index>(merged.vertex_positions_.size());
    if (writeMsh2_2(path, merged)) {
        spdlog::info("GmshModelHandler: wrote msh file: {} (components_merged={})",
            path.string(), merged_count);
    } else {
        spdlog::error("GmshModelHandler: failed to write msh file: {}", path.string());
    }
}

std::vector<ArgType> GmshModelHandler::read_args_type() const
{
    return {};
}

std::vector<ArgType> GmshModelHandler::write_args_type() const
{
    return {};
}
}
