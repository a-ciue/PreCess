#include "CTMeshModel.h"
#include "MeshData.h"
#include "ToolMesh.h"
#include "OBJMeshIO.h"
#include "TempFile.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
bool parseDoubleList(const std::string& text, std::vector<double>& values)
{
    std::istringstream stream(text);
    double value {};
    while (stream >> value)
        values.push_back(value);

    return !values.empty();
}

// 解析 .m 顶点 {...} 中的数值属性，例如 rgb=(1 0 0)、uv=(0.5 0.2)、wid=12。
std::unordered_map<std::string, std::vector<double>> parseVertexStringAttributes(const std::string& text)
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
            result["v_" + key + "_" + std::to_string(values.size())] = std::move(values);
    }
    return result;
}

void appendVertexAttributes(
    std::map<std::string, std::vector<double>>& vertex_attributes,
    std::unordered_map<std::string, size_t>& component_counts,
    const std::unordered_map<std::string, std::vector<double>>& parsed_attributes,
    size_t vertex_index)
{
    for (auto& [name, values] : vertex_attributes) {
        const size_t component_count = component_counts[name];
        values.resize((vertex_index + 1) * component_count, 0.0);
    }

    for (const auto& [name, tuple] : parsed_attributes) {
        const size_t component_count = tuple.size();
        auto component_iter = component_counts.find(name);
        if (component_iter == component_counts.end()) {
            component_counts[name] = component_count;
        } else if (component_iter->second != component_count) {
            continue;
        }

        auto& values = vertex_attributes[name];
        values.resize((vertex_index + 1) * component_count, 0.0);
        std::copy(tuple.begin(), tuple.end(), values.begin() + vertex_index * component_count);
    }
}
}

void CTMeshModel::update(MeshData& mesh_data)
{
    using namespace std;

    // 构造点坐标数组 MeshData::vertex_positions_
    auto& vertex_positions = mesh_data.vertex_positions_;
    vertex_positions.clear(); // 清空之前的顶点数据
    mesh_data.vertex_attributes_.clear();
    vertex_positions.reserve(mesh_->numVertices()); // 预留空间以提高性能
    unordered_map<Index, Index> vertex_index_map; // 顶点 ID 到索引的映射
    unordered_map<string, size_t> vertex_attribute_components;
    for (MeshLib::CTMesh::MeshVertexIterator vi(mesh_); !vi.end(); ++vi) {
        vertex_index_map[vi.value()->id()] = vertex_positions.size();
        const CPoint& point = vi.value()->point();
        vertex_positions.emplace_back(array { point[0], point[1], point[2] });
        appendVertexAttributes(
            mesh_data.vertex_attributes_,
            vertex_attribute_components,
            parseVertexStringAttributes(vi.value()->string()),
            vertex_positions.size() - 1);
    }
    for (auto& [name, values] : mesh_data.vertex_attributes_)
        values.resize(vertex_positions.size() * vertex_attribute_components[name], 0.0);

    // MeshData包括的patch id
    unordered_set<int> data_patch_ids;
    for (const auto& patch : mesh_data.patches_) {
        data_patch_ids.insert(patch.first);
    }

    // 按g将面分组
    std::unordered_map<int, std::vector<MeshLib::CTMesh::CFace*>> patch_faces;
    for (MeshLib::CTMesh::MeshFaceIterator fit(mesh_); !fit.end(); fit++) {
        int face_patch_id = fit.value()->get_g();
        patch_faces[face_patch_id].push_back(*fit);
    }

    // 遍历每个组更新面
    mesh_data.face_vertices_.clear();
    mesh_data.face_vertices_offset_ = { 0 };
    mesh_data.face_vertices_offset_.reserve(mesh_->numFaces());
    for (const auto& [patch_id, faces] : patch_faces) {
        // 初始化 patches_[patch_id]
        auto& patch = mesh_data.patches_[patch_id];
        if (!patch) {
            // 新增patch需要判断是否需要新增Block，默认block id为patch_id
            auto& block = mesh_data.blocks_[patch_id];
            if (!block) {
                block = std::make_unique<Block>();
                block->id = patch_id;
            }
            block->patchIDs.insert(patch_id);

            patch = std::make_unique<Patch>(patch_id, patch_id);
        }

        // 从数据中移除已处理的patch id
        data_patch_ids.erase(patch_id); 

        // 遍历面更新：MeshData::face_vertices_, Patch::faces
        patch->faces.clear(); // 清空之前的面片信息
        patch->faces.reserve(faces.size()); // 预留空间以提高性能
        for (auto& face : faces) {
            patch->faces.emplace_back(static_cast<Index>(mesh_data.face_vertices_offset_.size() - 1)); // 存面索引

            int i = 0;
            // 添加新面的点
            for (MeshLib::CTMesh::FaceVertexIterator vi(face); !vi.end(); ++vi) {
                auto& cur_index = mesh_data.face_vertices_.emplace_back();
                cur_index = vertex_index_map[vi.value()->id()]; // 存点索引
                ++i;
            }

            mesh_data.face_vertices_offset_.push_back(i + mesh_data.face_vertices_offset_.back());
        }
    }

    // 处理MeshData没有被更新的 Patch，应该被删除
    for (const auto& patch_id : data_patch_ids) {
        if (mesh_data.patches_.count(patch_id)) {
            mesh_data.patches_.erase(patch_id);
        }
    }

    // 维护Block
    for (auto& [block_id, block] : mesh_data.blocks_)
    {
        // Block只存现有Patch
        for (auto& cur_patch : block->patchIDs)
        {
            if (!patch_faces.count(cur_patch))
            {
                block->patchIDs.erase(cur_patch);
            }
        }
        
        if (block->patchIDs.empty())
        {
            mesh_data.blocks_.erase(block_id);
        }
    }
}

void CTMeshModel::updateFrom(const MeshData& mesh_data)
{
    // 解析mesh_data更新CTMesh
    auto temp_path = core::TempFile::instance().path();
    std::ofstream ofs(temp_path);
    ObjMeshIO::saveToFile(mesh_data, ofs);
    ofs.flush();
    ofs.close();
    mesh_->read_obj(temp_path.string().c_str());
}

void CTMeshModel::update(MeshData& mesh_data, const std::unordered_set<Index>& patch_ids)
{
}

CTMeshModel::CTMeshModel(MeshLib::CTMesh& mesh)
    :mesh_(&mesh)
{
}

CTMeshModel::~CTMeshModel() = default;