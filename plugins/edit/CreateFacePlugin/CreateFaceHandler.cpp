#include "CreateFaceHandler.h"
#include "ArgObject.h"
#include "ArgType.h"
#include "ComponentData.h"
#include "ComponentOperator.h"
#include "MeshData.h"
#include "MeshIDMap.h"
#include "ModelLayer.h"
#include "Selection.h"

#include <filesystem>
#include <spdlog/fmt/ranges.h>
#include <spdlog/spdlog.h>

namespace systems::edit {
using namespace core;
std::any CreateFaceHandler::execute(ModelLayer& model, Index /*fallback_component_id*/, const std::vector<core::ArgObject>& args)
{
    // 参数检查
    auto selection_p = args[0].get<ArgTypeEnum::Selector>();
    if (!selection_p || !*selection_p) {
        spdlog::error("CreateFaceHandler::execute: Argument 1 is missing or of wrong type.");
        return { };
    }
    auto selection = *selection_p;
    if (selection->type != ElementEnum::Vertex || selection->ids.size() < 3) {
        spdlog::error("CreateFaceHandler::execute: Selection type is not Vertex or vertices are less than 3.");
        return { };
    }

    // 选择集携带全局点 id：经 pointIdMap 反查所属组件与局部点索引，
    // 目标组件由全局 id 决定，不依赖对象树选中组件
    std::vector<Index> local_ids;
    local_ids.reserve(selection->ids.size());
    Index component_id = -1;
    for (Index gid : selection->ids) {
        const auto [cid, local] = model.pointIdMap().getLocal(gid);
        if (cid == MeshIDMap::kInvalidComponent) {
            spdlog::error("CreateFaceHandler::execute: Global point id {} is invalid.", gid);
            return { };
        }
        if (component_id == -1) {
            component_id = cid;
        } else if (cid != component_id) {
            spdlog::error("CreateFaceHandler::execute: Selected point {} belongs to component {}, expected component {}.",
                gid, cid, component_id);
            return { };
        }
        local_ids.push_back(local);
    }

    auto target_op = model.getComponentOperator(component_id);
    if (!target_op || !target_op->mesh()) {
        spdlog::error("CreateFaceHandler::execute: Component {} has no mesh.", component_id);
        return { };
    }

    spdlog::debug("CreateFaceHandler::execute: Creating face on component {} with points ID {}",
        component_id, selection->ids);

    // 追加面单元（写必脏：标脏 + 通知由操作边界 flush 统一发出）
    target_op->appendFace(local_ids);

    return { };
}

std::vector<ArgType> CreateFaceHandler::args_type() const
{
    return {
        ArgType { ArgTypeEnum::Selector, "选择点", "" }
    };
}
}