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
    // 按 OBJ shape(group) 拆分好的组件列表，失败时返回 std::nullopt
    auto comps = ObjMeshIO::loadFromFile(path);
    if (!comps) {
        return std::nullopt;
    }

    return ModelPayload{ path.filename().string(), std::move(*comps) };
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
    return {};
}

std::vector<ArgType> OBJModelHandler::write_args_type() const
{
    return {};
}
}
