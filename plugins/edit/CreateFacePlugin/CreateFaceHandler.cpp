#include "CreateFaceHandler.h"
#include "ArgObject.h"
#include "ArgType.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "ComponentData.h"         
#include "ComponentOperator.h"  
#include "Selection.h" 

#include <filesystem>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ranges.h>

namespace systems::edit {
using namespace core;
std::any CreateFaceHandler::execute(ComponentOperator& op, const std::vector<core::ArgObject>& args)
{
    // 参数检查
    ComponentData& comp = op.component();
    MeshData* mesh = comp.mesh.get();
    if (!mesh) {
        spdlog::error("CreateFaceHandler::execute: Current component has no mesh.");
        return {}; // 返回空
    }
    auto selection_p = args[0].get<ArgTypeEnum::Selector>();
    if (!selection_p || !*selection_p) {
        spdlog::error("CreateFaceHandler::execute: Argument 1 is missing or of wrong type.");
        return {};
    }
    auto selection = *selection_p;
    if (selection->type != ElementEnum::Vertex || selection->ids.size() < 3) {
        spdlog::error("CreateFaceHandler::execute: Selection type is not Vertex or vertices are less than 3.");
        return {};
    }

    spdlog::debug("CreateFaceHandler::execute: Creating face on component {} with points ID {}",
        op.componentId(), selection->ids);

    // 选择集携带全局点 id，换算为本组件局部点索引（face_vertices_ 的键空间）后再写入
    std::vector<Index> local_ids;
    local_ids.reserve(selection->ids.size());
    for (Index gid : selection->ids) {
        const auto [cid, local] = op.manager().pointIdMap().getLocal(gid);
        if (cid != op.componentId()) {
            spdlog::error("CreateFaceHandler::execute: Selected point {} does not belong to component {}.",
                gid, op.componentId());
            return {};
        }
        local_ids.push_back(local);
    }

    // 追加面对应的顶点索引 face_vertices_
    mesh->face_vertices_.insert(mesh->face_vertices_.end(), local_ids.begin(), local_ids.end());

    // 更新 face_vertices_offset_
    mesh->face_vertices_offset_.push_back(static_cast<Index>(mesh->face_vertices_.size()));

    return {};
}

std::vector<ArgType> CreateFaceHandler::args_type() const
{
    return {
        ArgType { ArgTypeEnum::Selector, "选择点", "" }
    };
}
}