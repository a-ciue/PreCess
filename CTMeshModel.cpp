#include "MeshData.h"
#include "ToolMesh.h"

/**
 * @brief 模型层辅助数据结构 CTMesh
 */
class CTMeshModel {
public:
    CTMeshModel();
    ~CTMeshModel();

    void updateMesh(MeshData& mesh_data)
    {
        // 提取所有 patch_id
        std::unordered_set<int> patch_ids;
        for (auto& face : mesh_.faces()) {
            patch_ids.insert(face->get_g());
        }

        // 更新 patches_
        mesh_data.update_patches(patch_ids);

        // 初始化 blocks_
        for (const auto& [patch_id, patch_ptr] : mesh_data.patches_) {
            int block_id = patch_id;
            mesh_data.patches_[patch_id]->blockID = block_id;

            if (mesh_data.blocks_.find(block_id) == mesh_data.blocks_.end()) {
                mesh_data.blocks_[block_id] = std::make_unique<Block>();
                mesh_data.blocks_[block_id]->id = block_id;
            }
            mesh_data.blocks_[block_id]->patchIDs.insert(patch_id);
        }
    }

    void update_patches(MeshData& mesh_data, const std::unordered_set<Index>& patch_ids)
    {
        // 删除指定的 Patch 数据，但保持Patch所在的BlockID
        std::unordered_map<int, int> blockIDs;
        for (int patch_id : patch_ids) {
            if (mesh_data.patches_.count(patch_id)) {
                blockIDs[patch_id] = mesh_data.patches_[patch_id]->blockID;
            }
        }

        // 分组面片：按 Patch ID 将面片分组，取patch_ids包括的patch
        std::unordered_map<int, std::vector<MeshLib::CTMesh::CFace*>> patch_faces;
        for (auto& face : mesh_.faces()) {
            int face_patch_id = face->get_g();
            if (patch_ids.count(face_patch_id)) {
                patch_faces[face_patch_id].push_back(face);
            }
        }

        // 遍历每个 Patch 的面片组
        for (const auto& [patch_id, faces] : patch_faces) {
            // 初始化 patches_[patch_id]
            auto& patch = mesh_data.patches_[patch_id];
            if (!patch) {
                Index block_id = blockIDs.count(patch_id) ? blockIDs[patch_id] : -1;
                patch = std::make_unique<Patch>(patch_id, block_id);
            }

            // 追踪 Patch 内部的顶点
            std::unordered_map<int, int> vertex_id_map; // 全局顶点 ID 到 Patch 内局部索引的映射

            // TODO: 处理更新MeshData的顶点和面片
        }
    }

private:
    MeshLib::CTMesh mesh_;
};

CTMeshModel::CTMeshModel()
{
}

CTMeshModel::~CTMeshModel()
{
}