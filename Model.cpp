/*
 * Model构造函数中需要调用ModelActor的构造函数和析构函数
 * 行17：In template: calling a private destructor of class 'ModelActor'
 * 行57：In template: calling a private constructor of class 'ModelActor'
 * 目前ModelActor的构造、析构函数仍为private
 * 一个解决方法：将ModelActor的构造、析构函数设置为public
*/
#include "Model.h"
#include "ModelActor.h"
#include "ToolMesh.h"
#include "ModelUtil.h"

#include <stdexcept>  // 用于抛出异常


Model::Model(std::unique_ptr<MeshLib::CTMesh> mesh)
        : mesh_(std::move(mesh)) // 初始化 mesh_
{
    if (!mesh_) {
        throw std::runtime_error("CTMesh pointer cannot be null.");
    }

    // 提取所有 patch_id
    std::unordered_set<int> patch_ids;
    //std::vector<int> patch_ids_vector;
    for (auto& face : mesh_->faces()) {

        //patch_ids_vector.push_back(face->get_g());
        patch_ids.insert(face->get_g());
    }

    // 更新 patches_
    update_patches(patch_ids);

    // 初始化 blocks_

    for (const auto& [patch_id, patch_ptr] : patches_) {
        //int block_id = generate_block_id(patch_id);  根据 patch_id 生成 block_id。这里的generate_block_id(int)是伪代码
        //可以先block_id = patch_id
        int block_id = patch_id;
        if (blocks_.find(block_id) == blocks_.end()) {
            blocks_[block_id] = std::make_unique<Block>();
            blocks_[block_id]->id = block_id;
        }
        blocks_[block_id]->patchIDs.insert(patch_id);
    }

    // 初始化 groups_
    /*
    for (const auto& [block_id, block_ptr] : blocks_) {
        int group_id = generate_group_id(block_id); // 根据 block_id 生成 group_id。这里的generate_group_id(int)是伪代码
        if (groups_.find(group_id) == groups_.end()) {
            groups_[group_id] = std::make_unique<Group>();
            groups_[group_id]->id = group_id;
        }
        groups_[group_id]->blockIDs.insert(block_id);
    }*/

    // 初始化 ModelActor
    actor_ = std::make_unique<ModelActor>(patches_, blocks_, groups_);
}


void Model::merge_blocks(const std::vector<int>& block_ids) {
    if (block_ids.empty()) {
        throw std::invalid_argument("block_ids cannot be empty.");
    }

    /*验证 block_ids 是否有效
    for (int id : block_ids) {
        if (blocks_.find(id) == blocks_.end()) {
            throw std::runtime_error("Block ID not found: " + std::to_string(id));
        }
    }*/

    // 获取目标 block（第一个 block）
    int target_block_id = block_ids[0];
    auto& target_block = blocks_[target_block_id];

    // 合并其他 block 的内容到目标 block
    for (size_t i = 1; i < block_ids.size(); ++i) {
        int id = block_ids[i];
        auto& block_to_merge = blocks_[id];

        // 合并 patchIDs
        for (int patch_id : block_to_merge->patchIDs) {
            target_block->patchIDs.insert(patch_id);
        }

        // 删除已合并的 block
        blocks_.erase(id);
    }

    // 更新 ModelActor
    // 更新目标 block 的 patchIDs
    // 调用 ModelActor 的 merge_blocks 函数更新 Actor
    actor_->merge_blocks(block_ids, block_ids[0], target_block->patchIDs);
}




void Model::merge_groups(const std::vector<int>& group_ids) {
    if (group_ids.empty()) {
        throw std::invalid_argument("group_ids cannot be empty.");
    }

    /* 验证 group_ids 是否有效
    for (int id : group_ids) {
        if (groups_.find(id) == groups_.end()) {
            throw std::runtime_error("Group ID not found: " + std::to_string(id));
        }
    }*/

    // 获取目标 group（第一个 group）
    int target_group_id = group_ids[0];
    auto& target_group = groups_[target_group_id];

    // 合并其他 group 的内容到目标 group
    for (size_t i = 1; i < group_ids.size(); ++i) {
        int id = group_ids[i];
        auto& group_to_merge = groups_[id];

        // 合并 blockIDs
        for (int block_id : group_to_merge->blockIDs) {
            target_group->blockIDs.insert(block_id);
        }

        // 删除已合并的 group
        groups_.erase(id);
    }

    // 更新 ModelActor
    //actor_->update_group(target_group_id, target_group->blockIDs); // 假设 ModelActor 有更新 group 的方法
    actor_->merge_groups(group_ids, group_ids[0], target_group->blockIDs);
}





void Model::remesh_block(int block_id) {
    // 验证 block_id 是否有效
    if (blocks_.find(block_id) == blocks_.end()) {
        throw std::runtime_error("Block ID not found: " + std::to_string(block_id));
    }

    // 获取指定 block
    auto& block = blocks_[block_id];

    // 将 block 的所有 patch_id 收集为一个向量
    std::vector<int> patch_ids(block->patchIDs.begin(), block->patchIDs.end());

    // 调用 ModelUtil::remesh_patches 方法对 patch 重新网格化
    mesh_ = ModelUtil::remesh_patches(std::move(mesh_), patch_ids);

    // 更新所有相关的 patches
    update_patches(patch_ids);

    // 更新所有相关的 actors
    update_actors(patch_ids);
}

void Model::remesh_group(int group_id) {
    // 验证 group_id 是否有效
    if (groups_.find(group_id) == groups_.end()) {
        throw std::runtime_error("Group ID not found: " + std::to_string(group_id));
    }

    // 获取指定 group
    auto& group = groups_[group_id];

    // 收集所有 patch_ids
    std::unordered_set<int> patch_ids_set;
    for (int block_id : group->blockIDs) {
        if (blocks_.find(block_id) == blocks_.end()) {
            throw std::runtime_error("Block ID not found in group: " + std::to_string(block_id));
        }
        auto& block = blocks_[block_id];
        patch_ids_set.insert(block->patchIDs.begin(), block->patchIDs.end());
    }

    // 将 patch_ids_set 转换为 vector
    std::vector<int> patch_ids(patch_ids_set.begin(), patch_ids_set.end());

    // 调用 ModelUtil::remesh_patches 方法对这些 patch 进行重新网格化
    mesh_ = ModelUtil::remesh_patches(std::move(mesh_), patch_ids);

    // 更新所有相关的 patches
    update_patches(patch_ids);

    // 更新所有相关的 actors
    update_actors(patch_ids);
}


int Model::face_patch_id(int face_id) {
    // 遍历所有 patches
    for (const auto& [patch_id, patch_ptr] : patches_) {
        if (std::find(patch_ptr->faceIDs_.begin(), patch_ptr->faceIDs_.end(), face_id) != patch_ptr->faceIDs_.end()) {
            return patch_id; // 找到对应的 patch_id
        }
    }

    // 如果找不到 face_id，抛出异常或返回特殊值
    throw std::runtime_error("Face ID not found in any patch.");
}

const std::vector<int>& Model::patch_face_ids(int patch_id) {
    // 检查 patch_id 是否存在
    if (patches_.find(patch_id) == patches_.end()) {
        throw std::runtime_error("Patch ID not found: " + std::to_string(patch_id));
    }

    // 返回对应 patch 的 faceIDs_
    return patches_[patch_id]->faceIDs_;
}

const std::vector<int>& Model::patch_vertex_ids(int patch_id) {
    // 检查 patch_id 是否存在
    if (patches_.find(patch_id) == patches_.end()) {
        throw std::runtime_error("Patch ID not found: " + std::to_string(patch_id));
    }

    // 返回对应 patch 的 vertexIDs_
    return patches_[patch_id]->vertexIDs_;
}

int Model::patch_block_id(int patch_id) {
    // 遍历 blocks_ 查找包含 patch_id 的 block
    for (const auto& [block_id, block_ptr] : blocks_) {
        if (block_ptr->patchIDs.find(patch_id) != block_ptr->patchIDs.end()) {
            return block_id; // 找到对应的 block_id
        }
    }

    // 如果找不到 patch_id，抛出异常
    throw std::runtime_error("Patch ID not found in any block.");
}

int Model::block_group_id(int patch_id) {
    // 先获取 patch 对应的 block_id
    int block_id = patch_block_id(patch_id);

    // 遍历 groups_ 查找包含 block_id 的 group
    for (const auto& [group_id, group_ptr] : groups_) {
        if (group_ptr->blockIDs.find(block_id) != group_ptr->blockIDs.end()) {
            return group_id; // 找到对应的 group_id
        }
    }

    // 如果找不到 block_id，抛出异常
    throw std::runtime_error("Block ID not found in any group.");
}

ModelActor& Model::actor() {
    // 检查 actor_ 是否有效
    if (!actor_) {
        throw std::runtime_error("ModelActor is not initialized.");
    }

    // 返回 actor 的引用
    return *actor_;
}
void Model::update_actors(const std::vector<int>& patch_ids)
{
    std::unordered_set<int> block_ids, group_ids;
    for (int patch_id : patch_ids) {
        block_ids.insert(patch_block_id(patch_id));
        actor_->update_patch(patch_id, patches_[patch_id]->vertexPoints_, patches_[patch_id]->faceTriangles_);
    }
    for (int block_id : block_ids) {
        group_ids.insert(block_group_id(block_id));
        actor_->update_block(block_id, blocks_[block_id]->patchIDs);
    }
    for (int group_id : group_ids)
    {
        actor_->update_group(group_id, groups_[group_id]->blockIDs);
    }
}
