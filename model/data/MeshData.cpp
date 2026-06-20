//
// Created by 徐昊阳 on 5/20/25.
//
#include "MeshData.h"
#include "MeshIDMap.h"
#include <spdlog/spdlog.h>

void MeshData::clear()
{
    vertex_positions_.clear();
    vertex_count_ = 0;
    local_to_global_.clear();
    face_vertices_.clear();
    face_vertices_offset_.clear();
    edge_vertices_.clear();
    local_to_global_edge_id.clear();
    solid_types_.clear();
    solid_vertices_.clear();
    solid_vertices_offset_.clear();
    solid_faces_vertices_.clear();
    solid_faces_vertices_offset_.clear();
    solid_faces_.clear();
    solid_faces_offset_.clear();
    patches_.clear();
    blocks_.clear();
    // 清除属性
    vertex_attributes_.clear();
    face_attributes_.clear();
    edge_attributes_.clear();
    solid_attributes_.clear();
}

void MeshData::init()
{
    clear();
    face_vertices_offset_.push_back(0);
    solid_vertices_offset_.push_back(0);
    solid_faces_vertices_offset_.push_back(0);
    solid_faces_offset_.push_back(0);
}

std::optional<Index> MeshData::face_patch_id(int face_id)
{
    // 遍历所有 patches
    for (const auto& [patch_id, patch_ptr] : this->patches_) {
        if (std::find(patch_ptr->faces.begin(), patch_ptr->faces.end(), face_id) != patch_ptr->faces.end()) {
            return patch_id; // 找到对应的 patch_id
        }
    }
    return {};
}

std::optional<Index> MeshData::patch_block_id(int patch_id)
{
    // 遍历 blocks_ 查找包含 patch_id 的 block
    for (const auto& [block_id, block_ptr] : this->blocks_) {
        if (block_ptr->patchIDs.find(patch_id) != block_ptr->patchIDs.end()) {
            return block_id; // 找到对应的 block_id
        }
    }
    return {};
}

void MeshData::makePointIdsGlobal()
{
    if (local_to_global_.empty())
        return;

    auto shift = [&](std::vector<Index>& a) {
        for (auto& x : a) {
            if (x >= 0 && x < (Index)local_to_global_.size())
                x = local_to_global_[x];
        }
    };

    shift(edge_vertices_);
    shift(face_vertices_);
    shift(solid_vertices_);
    shift(solid_faces_vertices_);
}

void MeshData::ensureEdgeIdMapBuilt(MeshIDMap& map, Index component_id)
{
    if (edge_vertices_.size() % 2 != 0) {
        spdlog::error("MeshData::ensureEdgeIdMapBuilt: edge_vertices_ size is odd, component_id={}", component_id);
        return;
    }

    const Index nEdges = static_cast<Index>(edge_vertices_.size() / 2);
    local_to_global_edge_id.resize(nEdges, -1);

    for (Index local_eid = 0; local_eid < nEdges; ++local_eid) {
        Index& gid = local_to_global_edge_id[local_eid];
        if (gid < 0) {
            gid = map.insert(component_id, local_eid);
        }
    }
}

void MeshData::releaseEdgeIdMap(MeshIDMap& map)
{
    for (Index gid : local_to_global_edge_id) {
        if (gid >= 0)
            map.remove(gid);
    }
    local_to_global_edge_id.clear();
}