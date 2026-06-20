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
static void localizePointIds(std::vector<Index>& ids, const std::unordered_map<Index, Index>& global_to_local)
{
    for (auto& x : ids) {
        auto it = global_to_local.find(x);
        if (it != global_to_local.end())
            x = it->second;
    }
}

static std::unique_ptr<MeshData> buildLocalMeshForExport(const ModelLayer& mgr, Index cid)
{
    const ComponentData* comp = mgr.findComponent(cid);
    if (!comp || !comp->mesh)
        return nullptr;

    const MeshData& src = *comp->mesh;

    const Index cnt = src.vertex_count_;
    if (cnt <= 0) {
        spdlog::error("MModelHandler: component {} has no vertices", cid);
        return nullptr;
    }
    if ((Index)src.local_to_global_.size() < cnt) {
        spdlog::error("MModelHandler: local_to_global_ size mismatch, cid={}, cnt={}, ltg={}",
            cid, cnt, src.local_to_global_.size());
        return nullptr;
    }

    const auto& gp = mgr.globalPoints();
    const Index gp_size = (Index)gp.size();

    std::unordered_map<Index, Index> global_to_local;
    for (Index i = 0; i < cnt; ++i) {
        const Index gid = src.local_to_global_[i];
        if (gid < 0 || gid >= gp_size) {
            spdlog::error("MModelHandler: local_to_global_ out of gp range, cid={}, i={}, gid={}, gp={}",
                cid, i, gid, gp_size);
            return nullptr;
        }
        global_to_local[gid] = i;
    }

    auto out = std::make_unique<MeshData>();
    out->init();

    out->vertex_positions_.reserve(cnt);
    for (Index i = 0; i < cnt; ++i) {
        out->vertex_positions_.push_back(gp[(size_t)src.local_to_global_[i]]);
    }
    out->vertex_count_ = cnt;

    out->edge_vertices_ = src.edge_vertices_;
    out->face_vertices_ = src.face_vertices_;
    out->solid_vertices_ = src.solid_vertices_;
    out->solid_faces_vertices_ = src.solid_faces_vertices_;

    out->face_vertices_offset_ = src.face_vertices_offset_;
    out->solid_types_ = src.solid_types_;
    out->solid_vertices_offset_ = src.solid_vertices_offset_;
    out->solid_faces_vertices_offset_ = src.solid_faces_vertices_offset_;
    out->solid_faces_ = src.solid_faces_;
    out->solid_faces_offset_ = src.solid_faces_offset_;

    localizePointIds(out->edge_vertices_, global_to_local);
    localizePointIds(out->face_vertices_, global_to_local);
    localizePointIds(out->solid_vertices_, global_to_local);
    localizePointIds(out->solid_faces_vertices_, global_to_local);

    for (const auto& [pid, p] : src.patches_) {
        if (!p)
            continue;
        auto np = std::make_unique<Patch>(p->id_, p->blockID);
        np->faces = p->faces;
        out->patches_[pid] = std::move(np);
    }
    for (const auto& [bid, b] : src.blocks_) {
        if (!b)
            continue;
        auto nb = std::make_unique<Block>();
        nb->id = b->id;
        nb->patchIDs = b->patchIDs;
        out->blocks_[bid] = std::move(nb);
    }

    out->vertex_attributes_ = src.vertex_attributes_;
    out->face_attributes_ = src.face_attributes_;
    out->edge_attributes_ = src.edge_attributes_;
    out->solid_attributes_ = src.solid_attributes_;

    out->local_to_global_edge_id.clear();

    return out;
}

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

    // 从运行期权威数据重建“局部 MeshData”，以适配 CTMeshModel::updateFrom
    auto local_mesh = buildLocalMeshForExport(mgr, cid);
    if (!local_mesh) {
        spdlog::error("MModelHandler: failed to build local mesh for export, cid={}", cid);
        return;
    }

    // MeshData -> CTMesh -> write .m
    MeshLib::CTMesh mesh;
    CTMeshModel model(mesh);
    model.updateFrom(*local_mesh);

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
