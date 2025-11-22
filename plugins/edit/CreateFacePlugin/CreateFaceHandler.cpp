#include "CreateFaceHandler.h"
#include "ArgObject.h"
#include "ArgType.h"
#include "MeshData.h"
#include "ModelOperatorBase.h"

#include <filesystem>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ranges.h>

namespace systems::edit {
using namespace core;
ModelData CreateFaceHandler::execute(ModelData data, const std::vector<ArgObject>& args)
{
    // 参数检查
    MeshData* mesh = data.asMeshData();
    if (!mesh) {
        spdlog::error("CreateFaceHandler::execute: ModelData is not a mesh.");
        return data;
    }
    auto selection_p = args[0].get<ArgTypeEnum::Selector>();
    if (!selection_p || !*selection_p) {
        spdlog::error("CreateFaceHandler::execute: Argument 1 is missing or of wrong type.");
        return data;
    }
    auto selection = *selection_p;
    if (selection->type != ElementEnum::Vertex || selection->ids.size() < 3) {
        spdlog::error("CreateFaceHandler::execute: Selection type is not Vertex or vertices are less than 3.");
        return data;
    }

    spdlog::debug("CreateFaceHandler::execute: Creating face with points ID {}", selection->ids);

    // 追加面对应的顶点索引 face_vertices_
    mesh->face_vertices_.insert(mesh->face_vertices_.end(), selection->ids.begin(), selection->ids.end());

    // 更新 face_vertices_offset_
    mesh->face_vertices_offset_.push_back(mesh->face_vertices_.size());

    return data;
}

std::vector<ArgType> CreateFaceHandler::args_type() const
{
    return {
        ArgType { ArgTypeEnum::Selector, "选择点", "" }
    };
}
}