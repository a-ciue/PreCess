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
static void localizePointIds(std::vector<Index>& ids, Index base)
{
    for (auto& x : ids)
        x -= base;
}

static std::unique_ptr<MeshData> buildLocalMeshForExport(const ModelLayer& mgr, const ComponentData& comp)
{
    if (!comp.mesh)
        return nullptr;

    const MeshData& src = *comp.mesh;

    const Index base = src.global_point_base_;
    const Index cnt = src.vertex_count_;
    if (base < 0 || cnt <= 0) {
        spdlog::error("MModelHandler: invalid mesh global point range base={}, cnt={}", base, cnt);
        return nullptr;
    }

    const auto& gp = mgr.globalPoints();
    if (base + cnt > (Index)gp.size()) {
        spdlog::error("MModelHandler: globalPoints out of range (base={}, cnt={}, gp={})", base, cnt, gp.size());
        return nullptr;
    }

    auto out = std::make_unique<MeshData>();
    out->init();

    // 1) 顶点坐标：从全局点池切片取回，作为“局部 mesh”的 vertex_positions_
    out->vertex_positions_.assign(gp.begin() + base, gp.begin() + base + cnt);
    out->vertex_count_ = cnt;
    out->global_point_base_ = -1; // 导出用局部 mesh，不需要全局 base
    out->point_ids_are_global_ = false; // 导出用局部编号

    // 2) 拓扑：拷贝并把点索引从 global -> local
    // 注意：commit 后 src 的点索引已经是全局点池编号
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

    // 将点索引 localize（只处理“引用点的数组”）
    // face_vertices_/edge_vertices_/solid_vertices_/solid_faces_vertices_ 都是点 id 数组
    if (src.point_ids_are_global_) {
        localizePointIds(out->edge_vertices_, base);
        localizePointIds(out->face_vertices_, base);
        localizePointIds(out->solid_vertices_, base);
        localizePointIds(out->solid_faces_vertices_, base);
    }

    // 3) Patch/Block（深拷贝，避免丢分组信息）
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

    // 4) 属性（直接拷贝）
    out->vertex_attributes_ = src.vertex_attributes_;
    out->face_attributes_ = src.face_attributes_;
    out->edge_attributes_ = src.edge_attributes_;
    out->solid_attributes_ = src.solid_attributes_;

    // 5) 导出不依赖 edge global id 映射
    out->local_to_global_edge_id.clear();

    return out;
}

std::unique_ptr<ModelData> MModelHandler::read_model(const fs::path& path, const std::vector<std::any>& args)
{
    // MeshData
    MeshLib::CTMesh mesh;
    mesh.read_m(path.string().c_str());
    auto mesh_data = std::make_unique<MeshData>();
    CTMeshModel model(mesh);
    model.update(*mesh_data);

    // ModelData
    auto model_data = std::make_unique<ModelData>(std::move(mesh_data));
    model_data->model_name_ = path.filename().string();

    return model_data;
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
    auto local_mesh = buildLocalMeshForExport(mgr, *comp);
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
