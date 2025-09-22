#include "CTMeshModel.h"
#include "MeshData.h"
#include "ToolMesh.h"
#include "OBJMeshIO.h"
#include "TempFile.h"

void CTMeshModel::update(MeshData& mesh_data)
{
    using namespace std;

    // 构造点坐标数组 MeshData::vertex_positions
    auto& vertex_positions = mesh_data.vertex_positions;
    vertex_positions.clear(); // 清空之前的顶点数据
    vertex_positions.reserve(mesh_->numVertices()); // 预留空间以提高性能
    unordered_map<Index, Index> vertex_index_map; // 顶点 ID 到索引的映射
    for (MeshLib::CTMesh::MeshVertexIterator vi(mesh_); !vi.end(); ++vi) {
        vertex_index_map[vi.value()->id()] = vertex_positions.size();
        const CPoint& point = vi.value()->point();
        vertex_positions.emplace_back(array { point[0], point[1], point[2] });
    }

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
    mesh_data.face_vertices.clear();
    mesh_data.face_vertex_offsets = { 0 };
    mesh_data.face_vertex_offsets.reserve(mesh_->numFaces());
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

        // 遍历面更新：MeshData::face_vertices, Patch::faces
        patch->faces.clear(); // 清空之前的面片信息
        patch->faces.reserve(faces.size()); // 预留空间以提高性能
        for (auto& face : faces) {
            patch->faces.emplace_back(mesh_data.face_vertex_offsets.size() - 1); // 存面索引

            int i = 0;
            // 添加新面的点
            for (MeshLib::CTMesh::FaceVertexIterator vi(face); !vi.end(); ++vi) {
                auto& cur_index = mesh_data.face_vertices.emplace_back();
                cur_index = vertex_index_map[vi.value()->id()]; // 存点索引
                ++i;
            }

            mesh_data.face_vertex_offsets.push_back(i + mesh_data.face_vertex_offsets.back());
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