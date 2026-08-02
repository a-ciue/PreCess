/**
 * @file MModelHandler.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "MModelHandler.h"
#include "ArgType.h"
#include "MeshData.h"
#include "ModelData.h"
#include "CTMeshModel.h"
#include "ToolMesh.h"
#include "ModelLayer.h"

#include <spdlog/spdlog.h>
#include <fstream>

namespace systems::io {
std::optional<ModelPayload> MModelHandler::read_model(const fs::path& path, const std::vector<std::any>& args)
{
    // MeshData
    MeshLib::CTMesh mesh;
    mesh.read_m(path.string().c_str());
    auto mesh_data = std::make_unique<MeshData>();
    CTMeshModel model(mesh);
    model.update(*mesh_data);

    auto c = std::make_unique<ComponentData>();
    c->id = -1;
    c->name = "Comp_0";
    c->mesh = std::move(mesh_data);

    ComponentDatas comps;
    comps.push_back(std::move(c));

    return ModelPayload{path.filename().string(), std::move(comps)};
}

void MModelHandler::write_components(const ModelLayer& mgr,
    const std::vector<Index>& component_ids,
    const fs::path& path,
    const std::vector<std::any>&)
{
    if (component_ids.empty()) {
        spdlog::error("MModelHandler::write_components: empty component_ids");
        return;
    }

    // .m：当前实现只支持导出单个 component
    if (component_ids.size() != 1) {
        spdlog::warn("MModelHandler: .m export only supports 1 component; got {}, will export the first one.",
            component_ids.size());
    }

    const Index cid = component_ids.front();

    const ComponentData* comp = mgr.findComponent(cid);
    if (!comp) {
        spdlog::error("MModelHandler: component {} not found", cid);
        return;
    }
    if (!comp->mesh) {
        spdlog::error("MModelHandler: component {} has no mesh, cannot export .m", cid);
        return;
    }

    // MeshData 现为自包含局部约定（vertex_positions_ 常驻坐标、连通性存局部点索引），
    // 无需再经全局点池重建，直接交给 CTMeshModel 导出
    // MeshData -> CTMesh -> write .m
    MeshLib::CTMesh mesh;
    CTMeshModel model(mesh);
    model.updateFrom(*comp->mesh);

    mesh.write_m(path.string().c_str());
}

std::vector<core::ArgType> MModelHandler::read_args_type() const
{
    return {};
}

std::vector<core::ArgType> MModelHandler::write_args_type() const
{
    return {};
}
}
