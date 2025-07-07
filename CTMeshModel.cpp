#include "CTMeshModel.h"
#include "MeshData.h"
#include "ModelUtil.h"
#include "ToolMesh.h"

void CTMeshModel::update(MeshData& mesh_data)
{
    using namespace std;

    // 初始化点坐标数组 MeshData::vertex_positions
    auto& vertex_positions = mesh_data.vertex_positions;
    vertex_positions.clear(); // 清空之前的顶点数据
    vertex_positions.reserve(mesh_->numVertices()); // 预留空间以提高性能
    unordered_map<Index, Index> vertex_index_map; // 顶点 ID 到索引的映射
    for (MeshLib::CTMesh::MeshVertexIterator vi(mesh_.get()); !vi.end(); ++vi) {
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
    for (auto& face : mesh_->faces()) {
        int face_patch_id = face->get_g();
        patch_faces[face_patch_id].push_back(face);
    }

    // 遍历每个组更新面
    mesh_data.face_vertices.clear(); // 清空之前的面片信息
    mesh_data.face_vertices.reserve(mesh_->numFaces()); // 预留空间以提高性能
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
            patch->faces.emplace_back(mesh_data.face_vertices.size()); // 存面索引

            auto& indices = mesh_data.face_vertices.emplace_back();
            int i = 0;
            for (MeshLib::CTMesh::FaceVertexIterator vi(face); !vi.end(); ++vi) {
                indices[i] = vertex_index_map[vi.value()->id()]; // 存点索引
                ++i;
            }
        }
    }

    // 处理MeshData没有被更新的 Patch，应该被删除
    for (const auto& patch_id : data_patch_ids) {
        if (mesh_data.patches_.count(patch_id)) {
            mesh_data.patches_.erase(patch_id);
        }
    }
}

void CTMeshModel::update(MeshData& mesh_data, const std::unordered_set<Index>& patch_ids)
{
}

CTMeshModel::CTMeshModel(const std::filesystem::path& mesh_path)
    : mesh_(ModelUtil::read_obj_with_groups(mesh_path))
{
}