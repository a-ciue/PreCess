/**
 * @file ModelData.cpp
 * @brief 实现 ModelData 类的核心功能，用于管理和操作网格数据
 *
 * 该文件包含 ModelData 类的实现，提供网格数据的存储、更新和操作功能，包括：
 * - 读取和写入网格数据
 * - 面和边的分割
 * - 块和组的合并
 * - 重新网格化功能
 * - 维护与 ModelActor 及 VTK 组件的交互
 *
 * @author 徐昊阳 haoyangxu06@gmail.com
 * @date 2025/3/8
 */

#include "ModelData.h"
#include "ToolMesh.h"
#include "ModelUtil.h"

#include <stdexcept>  // 用于抛出异常

ModelData::ModelData(MeshData mesh)
        : type_(Type::Mesh), data_(std::move(mesh))
{}

ModelData::ModelData(SplineData spline)
        : type_(Type::Spline), data_(std::move(spline))
{}

ModelData::Type ModelData::type() const { return type_; }
bool ModelData::isMesh()   const noexcept { return type_ == Type::Mesh; }
bool ModelData::isSpline() const noexcept { return type_ == Type::Spline; }

MeshData* ModelData::asMeshData() noexcept {
    return std::get_if<MeshData>(&data_);
}
const MeshData* ModelData::asMeshData() const noexcept {
    return std::get_if<MeshData>(&data_);
}

SplineData* ModelData::asSplineData() noexcept {
    return std::get_if<SplineData>(&data_);
}
const SplineData* ModelData::asSplineData() const noexcept {
    return std::get_if<SplineData>(&data_);
}

void ModelData::merge_blocks(Selection selection) {
    auto* md = asMeshData();
    auto& sel = selection;
    const std::vector<int>& block_ids = sel.ids;
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
    auto& target_block = md->blocks_[target_block_id];

    // 合并其他 block 的内容到目标 block
    for (size_t i = 1; i < block_ids.size(); ++i) {
        int id = block_ids[i];
        auto& block_to_merge = md->blocks_[id];

        // 合并 patchIDs
        for (int patch_id : block_to_merge->patchIDs) {
            target_block->patchIDs.insert(patch_id);
            md->patches_[patch_id]->blockID = target_block_id;
        }
        // 删除已合并的 block
        md->blocks_.erase(id);
    }
}

int ModelData::face_patch_id(int face_id) {
    auto* md = asMeshData();
    // 遍历所有 patches
    for (const auto& [patch_id, patch_ptr] : md->patches_) {
        if (std::find(patch_ptr->faces.begin(), patch_ptr->faces.end(), face_id) != patch_ptr->faces.end()) {
            return patch_id; // 找到对应的 patch_id
        }
    }

    // 如果找不到 face_id，抛出异常或返回特殊值
    throw std::runtime_error("Face ID not found in any patch.");
}

int ModelData::patch_block_id(int patch_id) {
    auto* md = asMeshData();
    // 遍历 blocks_ 查找包含 patch_id 的 block
    for (const auto& [block_id, block_ptr] : md->blocks_) {
        if (block_ptr->patchIDs.find(patch_id) != block_ptr->patchIDs.end()) {
            return block_id; // 找到对应的 block_id
        }
    }

    // 如果找不到 patch_id，抛出异常
    throw std::runtime_error("Patch ID not found in any block.");
}

std::optional<SplineDataVtk> ModelData::getSplineData()
{
    const auto* md = asSplineData();
    if (md) {
        SplineDataVtk modelData{ md->rootShape };
        return modelData;
    }
    return nullopt;
}
