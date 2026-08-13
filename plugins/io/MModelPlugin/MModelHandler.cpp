/**
 * @file MModelHandler.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "MModelHandler.h"
#include "ArgType.h"
#include "MeshData.h"
#include "ModelData.h"
#include "ModelLayer.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
/**
 * @brief 从文本中解析 double 列表，例如 "(1 0 0)" 去掉括号后的内容
 */
bool parseDoubleList(const std::string& text, std::vector<double>& values)
{
    std::istringstream stream(text);
    double value { };
    while (stream >> value)
        values.push_back(value);

    return !values.empty();
}

/**
 * @brief 提取 .m 行内 {...} 属性段文本（首个 '{' 到末个 '}' 之间）
 */
std::string traitText(const std::string& line)
{
    const size_t begin = line.find('{');
    const size_t end = line.rfind('}');
    if (begin == std::string::npos || end == std::string::npos || end <= begin)
        return { };
    return line.substr(begin + 1, end - begin - 1);
}

/**
 * @brief 解析 .m 属性段中的 key=(v1 v2 ...) 数值属性，例如 g=(1)、rgb=(1 0 0)
 */
std::unordered_map<std::string, std::vector<double>> parseStringAttributes(const std::string& text)
{
    std::unordered_map<std::string, std::vector<double>> result;
    size_t pos = 0;

    while (pos < text.size()) {
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])))
            ++pos;

        const size_t key_begin = pos;
        while (pos < text.size() && text[pos] != '=' && !std::isspace(static_cast<unsigned char>(text[pos])))
            ++pos;
        if (key_begin == pos)
            break;

        const std::string key = text.substr(key_begin, pos - key_begin);
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])))
            ++pos;
        if (pos >= text.size() || text[pos] != '=') {
            while (pos < text.size() && !std::isspace(static_cast<unsigned char>(text[pos])))
                ++pos;
            continue;
        }
        ++pos;

        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])))
            ++pos;

        std::string value_text;
        if (pos < text.size() && text[pos] == '(') {
            const size_t value_begin = pos + 1;
            const size_t value_end = text.find(')', value_begin);
            if (value_end == std::string::npos)
                break;
            value_text = text.substr(value_begin, value_end - value_begin);
            pos = value_end + 1;
        } else {
            const size_t value_begin = pos;
            while (pos < text.size() && !std::isspace(static_cast<unsigned char>(text[pos])))
                ++pos;
            value_text = text.substr(value_begin, pos - value_begin);
        }

        std::vector<double> values;
        if (parseDoubleList(value_text, values))
            result[key] = std::move(values);
    }
    return result;
}

/**
 * @brief 将解析出的属性按 <前缀><key>_<分量数> 命名写入属性表，缺失元素补 0
 *
 * 前缀约定同 MeshData：顶点属性 "v_"、面属性 "f_"。
 */
void appendAttributes(
    const std::string& prefix,
    std::map<std::string, std::vector<double>>& attributes,
    std::unordered_map<std::string, size_t>& component_counts,
    const std::unordered_map<std::string, std::vector<double>>& parsed_attributes,
    size_t element_index)
{
    for (auto& [name, values] : attributes) {
        const size_t component_count = component_counts[name];
        values.resize((element_index + 1) * component_count, 0.0);
    }

    for (const auto& [key, tuple] : parsed_attributes) {
        const std::string name = prefix + key + "_" + std::to_string(tuple.size());
        const size_t component_count = tuple.size();
        auto component_iter = component_counts.find(name);
        if (component_iter == component_counts.end()) {
            component_counts[name] = component_count;
        } else if (component_iter->second != component_count) {
            continue;
        }

        auto& values = attributes[name];
        values.resize((element_index + 1) * component_count, 0.0);
        std::copy(tuple.begin(), tuple.end(), values.begin() + element_index * component_count);
    }
}

/**
 * @brief .m 属性段中的一个属性：key=(v1 v2 ...) 的还原形式
 */
struct MAttribute {
    std::string key;
    size_t components;
    const std::vector<double>* values;
};

/**
 * @brief 从 MeshData 属性表还原 .m 属性段属性列表
 *
 * 仅识别 <前缀><key>_<分量数> 命名且长度与元素数一致的属性，其余跳过。
 */
std::vector<MAttribute> collectAttributes(
    const std::map<std::string, std::vector<double>>& attributes,
    const std::string& prefix,
    size_t element_count)
{
    std::vector<MAttribute> result;
    for (const auto& [name, values] : attributes) {
        if (name.rfind(prefix, 0) != 0)
            continue;

        const size_t sep = name.rfind('_');
        if (sep == std::string::npos || sep <= prefix.size() || sep + 1 >= name.size())
            continue;

        // 末段须为全数字的分量数
        const std::string count_text = name.substr(sep + 1);
        if (!std::all_of(count_text.begin(), count_text.end(),
                [](unsigned char ch) { return std::isdigit(ch) != 0; }))
            continue;
        const size_t components = std::stoul(count_text);
        if (components == 0 || values.size() != element_count * components)
            continue;

        result.push_back({ name.substr(prefix.size(), sep - prefix.size()), components, &values });
    }
    return result;
}

/**
 * @brief 写出单个元素的 {...} 属性段（无属性时不写）
 */
void writeTrait(std::ostream& os, const std::vector<MAttribute>& attributes, size_t element_index)
{
    if (attributes.empty())
        return;

    os << " {";
    bool first = true;
    for (const MAttribute& attribute : attributes) {
        if (!first)
            os << " ";
        os << attribute.key << "=(";
        for (size_t k = 0; k < attribute.components; ++k) {
            if (k > 0)
                os << " ";
            os << (*attribute.values)[element_index * attribute.components + k];
        }
        os << ")";
        first = false;
    }
    os << "}";
}
}

namespace systems::io {
std::optional<ModelPayload> MModelHandler::read_model(const fs::path& path, const std::vector<std::any>& args)
{
    std::ifstream ifs(path);
    if (!ifs) {
        spdlog::error("MModelHandler: failed to open input file: {}", path.string());
        return std::nullopt;
    }

    auto mesh_data = std::make_unique<MeshData>();
    mesh_data->init();

    // .m 顶点/面 id 为文件内编号（通常从 1 开始），读入时重映射为组件内局部点索引
    std::unordered_map<Index, Index> vertex_index_map;
    std::unordered_map<std::string, size_t> vertex_attribute_components;
    std::unordered_map<std::string, size_t> face_attribute_components;

    std::string line;
    while (std::getline(ifs, line)) {
        std::istringstream line_stream(line);
        std::string keyword;
        if (!(line_stream >> keyword))
            continue;

        if (keyword == "Vertex") {
            Index vid { };
            std::array<double, 3> point { };
            if (!(line_stream >> vid >> point[0] >> point[1] >> point[2])) {
                spdlog::warn("MModelHandler: malformed Vertex line, skip: {}", line);
                continue;
            }

            const size_t local_index = mesh_data->vertex_positions_.size();
            vertex_index_map[vid] = static_cast<Index>(local_index);
            mesh_data->vertex_positions_.push_back(point);
            appendAttributes(
                "v_",
                mesh_data->vertex_attributes_,
                vertex_attribute_components,
                parseStringAttributes(traitText(line)),
                local_index);
        } else if (keyword == "Face") {
            Index fid { };
            if (!(line_stream >> fid)) {
                spdlog::warn("MModelHandler: malformed Face line, skip: {}", line);
                continue;
            }

            // 面点 id 列表在 {...} 属性段之前，流读取遇 '{' 失败自然结束
            std::vector<Index> face_vertices;
            Index vid { };
            bool valid = true;
            while (line_stream >> vid) {
                const auto iter = vertex_index_map.find(vid);
                if (iter == vertex_index_map.end()) {
                    spdlog::warn("MModelHandler: Face {} references unknown vertex {}, skip", fid, vid);
                    valid = false;
                    break;
                }
                face_vertices.push_back(iter->second);
            }
            if (!valid || face_vertices.empty())
                continue;

            const Index face_index = static_cast<Index>(mesh_data->face_vertices_offset_.size() - 1);
            mesh_data->face_vertices_.insert(
                mesh_data->face_vertices_.end(), face_vertices.begin(), face_vertices.end());
            mesh_data->face_vertices_offset_.push_back(
                static_cast<Index>(mesh_data->face_vertices_.size()));
            appendAttributes(
                "f_",
                mesh_data->face_attributes_,
                face_attribute_components,
                parseStringAttributes(traitText(line)),
                face_index);
        }
        // 其余行（Edge / Corner 等仅承载属性的记录）不携带几何信息，忽略
    }

    mesh_data->vertex_count_ = static_cast<Index>(mesh_data->vertex_positions_.size());
    // 属性数组补齐：未声明该属性的元素补 0
    for (auto& [name, values] : mesh_data->vertex_attributes_)
        values.resize(mesh_data->vertex_positions_.size() * vertex_attribute_components[name], 0.0);
    const size_t face_count = mesh_data->face_vertices_offset_.size() - 1;
    for (auto& [name, values] : mesh_data->face_attributes_)
        values.resize(face_count * face_attribute_components[name], 0.0);

    auto c = std::make_unique<ComponentData>();
    c->id = -1;
    c->name = "Comp_0";
    c->mesh = std::move(mesh_data);

    ComponentDatas comps;
    comps.push_back(std::move(c));

    return ModelPayload { path.filename().string(), std::move(comps) };
}

void MModelHandler::write_components(const ModelLayer& mgr,
    const std::vector<Index>& component_ids,
    const fs::path& path,
    const std::vector<std::any>&)
{
    if (component_ids.empty()) {
        spdlog::error("MModelHandler::write_components: empty component_ids");
        return;
    }

    // .m：当前实现只支持导出单个 component
    if (component_ids.size() != 1) {
        spdlog::warn("MModelHandler: .m export only supports 1 component; got {}, will export the first one.",
            component_ids.size());
    }

    const Index cid = component_ids.front();

    const ComponentData* comp = mgr.findComponent(cid);
    if (!comp) {
        spdlog::error("MModelHandler: component {} not found", cid);
        return;
    }
    if (!comp->mesh) {
        spdlog::error("MModelHandler: component {} has no mesh, cannot export .m", cid);
        return;
    }

    const MeshData& mesh = *comp->mesh;
    std::ofstream ofs(path);
    if (!ofs) {
        spdlog::error("MModelHandler: failed to open output file: {}", path.string());
        return;
    }

    // MeshData 自包含（vertex_positions_ 常驻坐标、连通性存局部点索引），
    // .m 顶点/面 id 从 1 开始，局部点索引 +1 即为文件顶点 id；
    // 点/面属性按 v_<key>_<分量数> / f_<key>_<分量数> 命名还原为 {...} 属性段
    const size_t face_count = mesh.face_vertices_offset_.empty() ? 0 : mesh.face_vertices_offset_.size() - 1;
    const std::vector<MAttribute> vertex_attributes = collectAttributes(mesh.vertex_attributes_, "v_", mesh.vertex_positions_.size());
    const std::vector<MAttribute> face_attributes = collectAttributes(mesh.face_attributes_, "f_", face_count);

    for (size_t i = 0; i < mesh.vertex_positions_.size(); ++i) {
        const auto& p = mesh.vertex_positions_[i];
        ofs << "Vertex " << i + 1 << " " << p[0] << " " << p[1] << " " << p[2];
        writeTrait(ofs, vertex_attributes, i);
        ofs << "\n";
    }
    for (size_t f = 0; f < face_count; ++f) {
        ofs << "Face " << f + 1;
        for (Index k = mesh.face_vertices_offset_[f]; k < mesh.face_vertices_offset_[f + 1]; ++k)
            ofs << " " << mesh.face_vertices_[k] + 1;
        writeTrait(ofs, face_attributes, f);
        ofs << "\n";
    }
}

std::vector<core::ArgType> MModelHandler::read_args_type() const
{
    return { };
}

std::vector<core::ArgType> MModelHandler::write_args_type() const
{
    return { };
}
}
