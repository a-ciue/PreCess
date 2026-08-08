//
// Created by 徐昊阳 on 5/20/25.
//
#include "MeshData.h"
#include <spdlog/spdlog.h>

std::unique_ptr<MeshData> MeshData::clone() const
{
    auto copy = std::make_unique<MeshData>();

    // 坐标与连通性
    copy->vertex_positions_ = vertex_positions_;
    copy->vertex_count_ = vertex_count_;
    copy->face_vertices_ = face_vertices_;
    copy->face_vertices_offset_ = face_vertices_offset_;
    copy->edge_vertices_ = edge_vertices_;
    copy->solid_types_ = solid_types_;
    copy->solid_vertices_ = solid_vertices_;
    copy->solid_vertices_offset_ = solid_vertices_offset_;
    copy->solid_faces_vertices_ = solid_faces_vertices_;
    copy->solid_faces_vertices_offset_ = solid_faces_vertices_offset_;
    copy->solid_faces_ = solid_faces_;
    copy->solid_faces_offset_ = solid_faces_offset_;

    // patches_/blocks_ 为待删的陈旧设施，不进快照

    // 属性 map
    copy->vertex_attributes_ = vertex_attributes_;
    copy->face_attributes_ = face_attributes_;
    copy->edge_attributes_ = edge_attributes_;
    copy->solid_attributes_ = solid_attributes_;

    return copy;
}

void MeshData::clear()
{
    vertex_positions_.clear();
    vertex_count_ = 0;
    face_vertices_.clear();
    face_vertices_offset_.clear();
    edge_vertices_.clear();
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