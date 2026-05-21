#include "DeleteFaceHandler.h"
#include "ArgObject.h"
#include "ArgType.h"
#include "MeshData.h"
#include "Selection.h"
#include "ComponentData.h"
#include "ComponentOperator.h"

#include <filesystem>
#include <execution>
#include <iostream>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ranges.h>

namespace systems::edit {
using namespace core;
std::any DeleteFaceHandler::execute(ComponentOperator& op, const std::vector<core::ArgObject>& args)
{
    // 参数检查
    ComponentData& comp = op.component();
    MeshData* mesh = comp.mesh.get();
    if (!mesh) {
        spdlog::error("DeleteFaceHandler::execute: Current component has no mesh.");
        return {};
    }
    auto selection_p = args[0].get<ArgTypeEnum::Selector>();
    if (!selection_p || !*selection_p) {
        spdlog::error("DeleteFaceHandler::execute: Argument 1 is missing or of wrong type.");
        return {};
    }
    auto selection = *selection_p;
    if (selection->type != ElementEnum::Face || selection->ids.empty()) {
        spdlog::error("DeleteFaceHandler::execute: Selection type is not Face or no faces selected.");
        return {};
    }

    // 获取所有需要删除的面ID并按降序排序，以便从后往前删除
    std::vector<Index> face_ids = selection->ids;
    std::sort(face_ids.begin(), face_ids.end(), std::greater<Index>());

    // 验证所有面ID都在有效范围内
    for (Index face_id : face_ids) {
        if (face_id < 0 || face_id >= static_cast<Index>(mesh->face_vertices_offset_.size() - 1)) {
            spdlog::error("DeleteFaceHandler::execute: Face ID {} is out of bounds.", face_id);
            return {};
        }
    }

    spdlog::debug("DeleteFaceHandler::execute: Deleting faces ID {} on component {}",
        face_ids, op.componentId());

   // 预先收集所有面的顶点数量（因为face_vertices_offset_会在删除过程中变化）
    std::vector<std::pair<Index, Index>> face_vertex_counts; // <face_id, vertices_count>
    for (Index face_id : face_ids) {
        Index vertices_count = mesh->face_vertices_offset_[face_id + 1] - mesh->face_vertices_offset_[face_id];
        face_vertex_counts.emplace_back(face_id, vertices_count);
    }

    // 按照从大到小的顺序删除面，避免索引变化影响
    for (const auto& [face_id, vertices_count] : face_vertex_counts) {
        // 删除面对应的顶点索引 face_vertices_
        auto vertices_from = mesh->face_vertices_.begin() + mesh->face_vertices_offset_[face_id];
        mesh->face_vertices_.erase(vertices_from, vertices_from + vertices_count);

        // 更新 face_vertices_offset_
        mesh->face_vertices_offset_.erase(mesh->face_vertices_offset_.begin() + face_id);
        std::for_each(std::execution::par, mesh->face_vertices_offset_.begin() + face_id, mesh->face_vertices_offset_.end(),
            [vertices_count](Index& offset) {
                offset -= vertices_count;
            });

        // 维护 patch 和 block 关系
        std::optional<Index> patch_id = mesh->face_patch_id(face_id); // 实时查询
        if (patch_id.has_value()) {
            std::vector<Index>& faces = mesh->patches_[patch_id.value()]->faces;
            auto it = std::find(faces.begin(), faces.end(), face_id);
            if (it != faces.end()) { // 确保找到了再删除
                faces.erase(it);
            }

            if (faces.empty()) {
                Index block_id = mesh->patches_[patch_id.value()]->blockID;
                std::unordered_set<Index>& patchIDs = mesh->blocks_[block_id]->patchIDs;
                patchIDs.erase(patch_id.value());
                if (patchIDs.empty()) {
                    mesh->blocks_.erase(block_id);
                }
            }
        }
    }

    // 对所有 patch 中的 face id 进行更新
    for (auto&& [_, patch] : mesh->patches_) {
        std::vector<Index>& faces = patch->faces;

        std::for_each(std::execution::par, faces.begin(), faces.end(),
            [&face_ids](Index& id) {
                // 计算需要减少的数量
                Index decrement = 0;
                for (Index deleted_id : face_ids) {
                    if (id > deleted_id) {
                        decrement++;
                    }
                }
                id -= decrement;
            });
    }

    return {};
}

std::vector<ArgType> DeleteFaceHandler::args_type() const
{
    return {
        ArgType { ArgTypeEnum::Selector, "选择面", "" }
    };
}
}