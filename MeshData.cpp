//
// Created by 徐昊阳 on 5/20/25.
//
#include "MeshData.h"

// 优化 update_patches 的实现，减少网格遍历次数
void MeshData::update_patches(const std::vector<Index>& patch_ids, bool new_patch) {
    // 使用 unordered_set 来处理 patch_ids 的快速查找
    std::unordered_set<int> patch_id_set(patch_ids.begin(), patch_ids.end());

    // 调用重载函数
    update_patches(patch_id_set, new_patch);
}