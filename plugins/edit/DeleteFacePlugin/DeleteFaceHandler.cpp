#include "DeleteFaceHandler.h"
#include "ArgObject.h"
#include "ArgType.h"
#include "MeshData.h"
#include "ModelOperatorBase.h"

#include <filesystem>
#include <execution>
#include <iostream>
#include <spdlog/spdlog.h>

namespace systems::edit {
using namespace core;
ModelData DeleteFaceHandler::execute(ModelData data, const std::vector<ArgObject>& args)
{
    // 参数检查
    MeshData* mesh = data.asMeshData();
    if (!mesh) {
        spdlog::error("DeleteFaceHandler::execute: ModelData is not a mesh.");
        return data;
    }
    auto selection_p = args[0].get<ArgTypeEnum::Selector>();
    if (!selection_p || !*selection_p) {
        spdlog::error("DeleteFaceHandler::execute: Argument 1 is missing or of wrong type.");
        return data;
    }
    auto selection = *selection_p;
    if (selection->type != ElementEnum::Face || selection->ids.empty()) {
        spdlog::error("DeleteFaceHandler::execute: Selection type is not Face or no faces selected.");
        return data;
    }

    Index face_id = selection->ids[0];
    if (face_id < 0 || face_id >= static_cast<Index>(mesh->face_vertices_offset_.size() - 1)) {
        spdlog::error("DeleteFaceHandler::execute: Face ID {} is out of bounds.", face_id);
        return data;
    }

    spdlog::debug("DeleteFaceHandler::execute: Deleting face ID {}", face_id);

    // 删除面对应的顶点索引 face_vertices_
    Index vertices_count = mesh->face_vertices_offset_[face_id + 1] - mesh->face_vertices_offset_[face_id];
    auto vertices_from = mesh->face_vertices_.begin() + mesh->face_vertices_offset_[face_id];
    mesh->face_vertices_.erase(vertices_from, vertices_from + vertices_count);

    // 更新 face_vertices_offset_
    mesh->face_vertices_offset_.erase(mesh->face_vertices_offset_.begin() + face_id);
    std::for_each(std::execution::par, mesh->face_vertices_offset_.begin() + face_id, mesh->face_vertices_offset_.end(),
        [vertices_count](Index& offset) {
            offset -= vertices_count;
        });

    // 维护 patch 和 block 关系
    if (std::optional patch_id = mesh->face_patch_id(face_id)) {
        std::vector<Index>& faces = mesh->patches_[*patch_id]->faces;
        auto it = std::find(faces.begin(), faces.end(), face_id);
        faces.erase(it);

        if (faces.empty()) {
            Index block_id = mesh->patches_[*patch_id]->blockID;
            std::unordered_set<Index>& patchIDs = mesh->blocks_[block_id]->patchIDs;
            patchIDs.erase(*patch_id);
            if (patchIDs.empty()) {
                mesh->blocks_.erase(block_id);
            }
        }
    }
    // 对所有 patch 中的 face id 进行更新
    for (auto&& [_, patch]: mesh->patches_) {
        std::vector<Index>& faces = patch->faces;

        std::for_each(std::execution::par, faces.begin(), faces.end(),
            [face_id](Index& id) {
                if (id > face_id) {
                    --id;
                }
            });
    }

    return data;
}

std::vector<ArgType> DeleteFaceHandler::args_type() const
{
    return {
        ArgType { ArgTypeEnum::Selector, "选择面", "" }
    };
}
}