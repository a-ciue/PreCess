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
#include <spdlog/spdlog.h>

ModelData::ModelData() = default;
ModelData::~ModelData() = default;

ModelData::ModelData(std::unique_ptr<MeshData> mesh)
{
    Component* c = createComponent(-1, "Comp_0");
    c->mesh = std::move(mesh);
    spdlog::info("ModelData: created 1 component with mesh");
}

ModelData::ModelData(std::unique_ptr<SplineData> spline)
{
    Component* c = createComponent(-1, "Comp_0");
    c->cad = std::move(spline);
    spdlog::info("ModelData: created 1 component with spline");
}

ModelData::ModelData(ModelData&& other) noexcept = default;
ModelData& ModelData::operator=(ModelData&& other) noexcept = default;

// 创建组件
Component* ModelData::createComponent(Index id, const std::string& name)
{
    auto c = std::make_unique<Component>();
    c->id = id;
    c->name = name;

    components_.push_back(std::move(c));
    return components_.back().get();
}
std::vector<std::unique_ptr<Component>>& ModelData::components() 
{ 
    return components_; 
}
const std::vector<std::unique_ptr<Component>>& ModelData::components() const 
{ 
    return components_; 
}

ModelData::Type ModelData::type() const
{
    bool anyMesh = false;
    bool anySpline = false;

    for (const auto& cPtr : components_) {
        if (!cPtr)
            continue;
        if (cPtr->mesh)
            anyMesh = true;
        if (cPtr->cad)
            anySpline = true;
    }

    if (!anyMesh && !anySpline)
        return Type::None;
    if (anyMesh && !anySpline)
        return Type::Mesh;
    if (!anyMesh && anySpline)
        return Type::Spline;
    return Type::Mixed; // 既有 mesh 组件又有 CAD 组件
}
bool ModelData::hasMesh() const noexcept
{
    Type t = type();
    return t == Type::Mesh || t == Type::Mixed;
}
bool ModelData::hasSpline() const noexcept
{
    Type t = type();
    return t == Type::Spline || t == Type::Mixed;
}

MeshData* ModelData::asMeshData() noexcept
{
    if (!hasMesh())
        return nullptr;
    for (const auto& cPtr : components_) {
        if (cPtr && cPtr->mesh)
            return cPtr->mesh.get();
    }
    return nullptr;
}
const MeshData* ModelData::asMeshData() const noexcept
{
    return const_cast<ModelData*>(this)->asMeshData();
}

SplineData* ModelData::asSplineData() noexcept
{
    if (!hasSpline())
        return nullptr;
    for (const auto& cPtr : components_) {
        if (cPtr && cPtr->cad)
            return cPtr->cad.get();
    }
    return nullptr;
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