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
#include "MeshData.h"
#include "SplineData.h"

#include <stdexcept>  // 用于抛出异常
#include <algorithm>

ModelData::ModelData() = default;
ModelData::~ModelData() = default;

ModelData::ModelData(std::unique_ptr<MeshData> mesh)
    : data_(std::move(mesh)) { }

ModelData::ModelData(std::unique_ptr<SplineData> spline)
    : data_(std::move(spline)) { }

ModelData::ModelData(ModelData&& other) noexcept = default;
ModelData& ModelData::operator=(ModelData&& other) noexcept = default;

ModelData::Type ModelData::type() const
{
    if (hasMesh()) {
        return Type::Mesh;
    }
    if (hasSpline()) {
        return Type::Spline;
    }
    return Type::None;
}
bool ModelData::hasMesh() const noexcept { return asMeshData(); }
bool ModelData::hasSpline() const noexcept { return asSplineData(); }

MeshData* ModelData::asMeshData() noexcept
{
    auto pp = std::get_if<std::unique_ptr<MeshData>>(&data_);
    return pp ? pp->get() : nullptr;
}
const MeshData* ModelData::asMeshData() const noexcept
{
    return const_cast<ModelData*>(this)->asMeshData();
}

SplineData* ModelData::asSplineData() noexcept
{
    auto pp = std::get_if<std::unique_ptr<SplineData>>(&data_);
    return pp ? pp->get() : nullptr;
}
const SplineData* ModelData::asSplineData() const noexcept
{
    return const_cast<ModelData*>(this)->asSplineData();
}

void ModelData::merge_blocks(Selection selection)
{
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