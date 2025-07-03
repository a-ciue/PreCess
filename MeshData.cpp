//
// Created by 徐昊阳 on 5/20/25.
//
#include "MeshData.h"
MeshData::MeshData(std::unique_ptr<MeshLib::CTMesh> mesh)
        : mesh_(std::move(mesh)) // 初始化 mesh_
{
    if (!mesh_) {
        throw std::runtime_error("CTMesh pointer cannot be null.");
    }

    // 提取所有 patch_id
    std::unordered_set<int> patch_ids;
    for (auto& face : mesh_->faces()) {
        patch_ids.insert(face->get_g());
    }

    // 更新 patches_
    update_patches(patch_ids);

    // 初始化 blocks_
    for (const auto& [patch_id, patch_ptr] : patches_) {
        int block_id = patch_id;
        patches_[patch_id]->blockID = block_id;

        if (blocks_.find(block_id) == blocks_.end()) {
            blocks_[block_id] = std::make_unique<Block>();
            blocks_[block_id]->id = block_id;
        }
        blocks_[block_id]->patchIDs.insert(patch_id);
    }
}

// 优化 update_patches 的实现，减少网格遍历次数
void MeshData::update_patches(const std::vector<int>& patch_ids, bool new_patch) {
    // 使用 unordered_set 来处理 patch_ids 的快速查找
    std::unordered_set<int> patch_id_set(patch_ids.begin(), patch_ids.end());

    // 调用重载函数
    update_patches(patch_id_set, new_patch);
}
void MeshData::update_patches(const std::unordered_set<int>& patch_ids, bool new_patch) {
    // 删除指定的 Patch 数据，但保持Patch所在的BlockID
    std::unordered_map<int, int> blockIDs;
    for (int patch_id : patch_ids) {
        if (!new_patch && !patches_.count(patch_id))
        {
            //throw exception(("patch not found" + std::to_string(patch_id)).c_str());
            throw std::runtime_error("patch not found" + std::to_string(patch_id));
        }

        if (patches_.count(patch_id))
        {
            blockIDs[patch_id] = patches_[patch_id]->blockID;
            patches_.erase(patch_id);
        }
    }

    // 分组面片：按 Patch ID 将面片分组，取patch_ids包括的patch
    std::unordered_map<int, std::vector<MeshLib::CTMesh::CFace*>> patch_faces;
    for (auto& face : mesh_->faces()) {
        int face_patch_id = face->get_g();
        if (patch_ids.find(face_patch_id) != patch_ids.end()) {
            patch_faces[face_patch_id].push_back(face);
        }
    }

    // 遍历每个 Patch 的面片组
    for (const auto& [patch_id, faces] : patch_faces) {
        // 初始化 Patch
        auto& patch = patches_[patch_id];
        if (!patch) {
            // 赋patch->blockID，从blockIDs取出
            if (!blockIDs.count(patch_id))
                blockIDs[patch_id] = -1;
            patch = std::make_unique<Patch>(patch_id, blockIDs[patch_id]);
            blockIDs.erase(patch_id);
        }

        // 追踪 Patch 内部的顶点
        std::unordered_map<int, int> vertex_id_map;  // 全局顶点 ID 到 Patch 内局部索引的映射

        // 遍历面片，更新 Patch 的顶点和三角形信息
        for (auto* face : faces) {
            // 添加当前面的三角形
            std::array<int, 3> triangle{};
            int i = 0;

            // 遍历面的顶点
            for (MeshLib::CTMesh::FaceVertexIterator vi(face); !vi.end(); ++vi) {
                auto vertex = *vi;

                // 如果顶点未被记录，添加到 Patch 的顶点列表
                if (vertex_id_map.find(vertex->id()) == vertex_id_map.end()) {
                    vertex_id_map[vertex->id()] = patch->vertexIDs_.size();
                    patch->vertexIDs_.push_back(vertex->id());

                    // 插入顶点坐标
                    CPoint& vp = vertex->point();
                    patch->vertexPoints_.emplace_back(std::array<double, 3>{vp[0], vp[1], vp[2]});
                }

                // 设置三角形索引
                triangle[i++] = vertex_id_map[vertex->id()];
            }

            // 添加三角形到 Patch 的索引列表
            patch->faceTriangles_.push_back(triangle);
            patch->faceIDs_.push_back(face->id());
        }
    }
}