/**
 * @file OBJModelHandler.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "OBJModelHandler.h"
#include "ArgType.h"
#include "MeshData.h"
#include "ModelData.h"
#include "ObjMeshIO.h"
#include "ModelLayer.h"

#include <spdlog/spdlog.h>
#include <fstream>

namespace systems::io {
using core::ArgType;
std::optional<ModelPayload> OBJModelHandler::read_model(const fs::path& path, const std::vector<std::any>& args)
{
    // 默认合并 g 分组，调用方可显式开启拆分。
    const bool split_groups = !args.empty() && std::any_cast<bool>(args.front());
    auto comps = ObjMeshIO::loadFromFile(path, split_groups);
    if (!comps) {
        return std::nullopt;
    }

    return ModelPayload{ path.filename().u8string(), std::move(*comps) };
}

void OBJModelHandler::write_components(const ModelLayer& mgr,
    const std::vector<Index>& component_ids,
    const fs::path& path,
    const std::vector<std::any>&)
{
    std::ofstream ofs(path);
    if (!ofs) {
        spdlog::error("OBJModelHandler: failed to open output file: {}", path.string());
        return;
    }

    // 按 id 从 ModelLayer 解析组件，写出逻辑复用 ObjMeshIO::saveToFile
    std::vector<const ComponentData*> components;
    for (Index cid : component_ids) {
        const ComponentData* comp = mgr.findComponent(cid);
        if (!comp) {
            spdlog::warn("OBJModelHandler: component {} not found, skip", cid);
            continue;
        }
        if (!comp->mesh) {
            spdlog::warn("OBJModelHandler: component {} has no mesh, skip", cid);
            continue;
        }
        if (comp->mesh->vertex_count_ <= 0) {
            spdlog::warn("OBJModelHandler: component {} has no vertices, skip", cid);
            continue;
        }
        components.push_back(comp);
    }

    ObjMeshIO::saveToFile(components, ofs);
}

std::vector<ArgType> OBJModelHandler::read_args_type() const
{
    return { { ArgTypeEnum::Bool, "按 g 分组拆分组件", "false",
        "关闭时合并分组的面。组名前缀作为面属性名，末尾的数字作为值，缺值填 0。" } };
}

std::vector<ArgType> OBJModelHandler::write_args_type() const
{
    return {};
}
}
