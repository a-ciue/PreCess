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
    if (!op.mesh()) {
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

    // 追加面单元（写必脏：标脏 + 通知由操作边界 flush 统一发出）
    op.appendFace(local_ids);

    return {};
}

std::vector<ArgType> CreateFaceHandler::args_type() const
{
    return {
        ArgType { ArgTypeEnum::Selector, "选择点", "" }
    };
}
}