/**
 * @file MeshMeditModelHandler.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "MeshMeditModelHandler.h"
#include "ArgType.h"
#include "MeshData.h"
#include "ModelData.h"
#include "ModelLayer.h"
#include "LibMeshbIO.h"

#include <libmeshb7.h>
#include <spdlog/spdlog.h>

#include <cstddef>
#include <fstream>
#include <optional>

namespace systems::io {
static inline std::optional<Index>
toFileVertexIdChecked(Index global_pid, const std::unordered_map<Index, Index>& global_to_local, Index file_vertex_offset)
{
    auto it = global_to_local.find(global_pid);
    if (it == global_to_local.end())
        return std::nullopt;
    return file_vertex_offset + it->second;
}

/**
 * @brief 把运行期某个 component 的 MeshData（点索引是全局点池编号、且 vertex_positions_ 已被清空）
 *        追加合并到 merged（merged 是导出用的“局部 mesh”，vertex_positions_ 有坐标、索引是文件内局部）。
 */
static bool appendComponentMeshToMerged(const ModelLayer& mgr,
    const ComponentData& comp,
    MeshData& merged,
    Index& io_file_vertex_offset /*in/out*/)
{
    if (!comp.mesh)
        return false;

    const MeshData& src = *comp.mesh;

    const Index cnt = src.vertex_count_;
    if (cnt <= 0) {
        spdlog::error("MeshMeditModelHandler: component {} has no vertices", comp.id);
        return false;
    }

    std::unordered_map<Index, Index> global_to_local;
    for (Index i = 0; i < cnt; ++i) {
        global_to_local[src.local_to_global_[i]] = i;
    }

    const auto& gp = mgr.globalPoints();

    // 1) 追加顶点坐标：从 globalPoints 通过 local_to_global_ 拷贝进 merged.vertex_positions_
    merged.vertex_positions_.reserve(merged.vertex_positions_.size() + cnt);
    for (Index i = 0; i < cnt; ++i) {
        merged.vertex_positions_.push_back(gp[static_cast<size_t>(src.local_to_global_[i])]);
    }

    // 2) 追加边：src.edge_vertices_ 每两个为一条边，点索引是“全局点 id”
    if (!src.edge_vertices_.empty()) {
        if (src.edge_vertices_.size() % 2 != 0) {
            spdlog::warn("MeshMeditModelHandler: edge_vertices_ size odd, skip edges, cid={}", comp.id);
        } else {
            merged.edge_vertices_.reserve(merged.edge_vertices_.size() + src.edge_vertices_.size());
            for (Index gid : src.edge_vertices_) {
                auto out = toFileVertexIdChecked(gid, global_to_local, io_file_vertex_offset);
                merged.edge_vertices_.push_back(*out);
            }
        }
    }

    // 3) 追加面：需要同时追加 face_vertices_ 与 face_vertices_offset_
    // merged.face_vertices_offset_ 必须保持“首元素 0 + 单调递增”
    if (src.face_vertices_offset_.size() >= 2) {
        const Index old_face_vert_size = static_cast<Index>(merged.face_vertices_.size());

        // 3.1 顶点索引追加
        merged.face_vertices_.reserve(merged.face_vertices_.size() + src.face_vertices_.size());
        for (Index gid : src.face_vertices_) {
            auto out = toFileVertexIdChecked(gid, base, cnt, io_file_vertex_offset);
            merged.face_vertices_.push_back(*out);
        }

        // 3.2 offset 追加：跳过 src 的第一个 0，从第二个开始逐个 + old_face_vert_size
        if (merged.face_vertices_offset_.empty())
            merged.face_vertices_offset_.push_back(0);

        for (size_t i = 1; i < src.face_vertices_offset_.size(); ++i) {
            merged.face_vertices_offset_.push_back(old_face_vert_size + src.face_vertices_offset_[i]);
        }
    }

    // 4) 追加体单元：solid_types_ + solid_vertices_ + solid_vertices_offset_
    if (src.solid_vertices_offset_.size() >= 2 && src.solid_types_.size() + 1 == src.solid_vertices_offset_.size()) {
        const Index old_solid_vert_size = static_cast<Index>(merged.solid_vertices_.size());

        merged.solid_vertices_.reserve(merged.solid_vertices_.size() + src.solid_vertices_.size());
        for (Index gid : src.solid_vertices_) {
            auto out = toFileVertexIdChecked(gid, base, cnt, io_file_vertex_offset);
            merged.solid_vertices_.push_back(*out);
        }

        merged.solid_types_.reserve(merged.solid_types_.size() + src.solid_types_.size());
        for (unsigned char t : src.solid_types_) {
            merged.solid_types_.push_back(t);
        }

        if (merged.solid_vertices_offset_.empty())
            merged.solid_vertices_offset_.push_back(0);

        for (size_t i = 1; i < src.solid_vertices_offset_.size(); ++i) {
            merged.solid_vertices_offset_.push_back(old_solid_vert_size + src.solid_vertices_offset_[i]);
        }

        // libmeshb 写出不依赖 solid_faces_*，但 MeshData::init() 要求这些 offset 至少有 {0}
        // 这里保持与“每个 solid push 一个 0”的风格一致
        if (merged.solid_faces_offset_.empty())
            merged.solid_faces_offset_.push_back(0);
        for (size_t i = 0; i < src.solid_types_.size(); ++i) {
            merged.solid_faces_offset_.push_back(0);
        }
    } else {
        // 没体单元或结构不一致则忽略体单元
        // （你现有数据里一般是 consistent 的）
    }

    // 更新 file 顶点偏移：下一个 component 的顶点从这里开始
    io_file_vertex_offset += cnt;
    return true;
}

std::optional<ModelPayload> MeshMeditModelHandler::read_model(const fs::path& path, const std::vector<std::any>& args)
{
    // MeshData
    auto mesh_data = std::make_unique<MeshData>();

    const bool success = LibMeshbIO::read(path, *mesh_data);
    if (!success) {
        spdlog::error("MeshMeditModelHandler: failed to read mesh from file: {}", path.string());
        return std::nullopt;
    }

    auto c = std::make_unique<ComponentData>();
    c->id = -1;
    c->name = "Comp_0";
    c->mesh = std::move(mesh_data);

    ComponentDatas comps;
    comps.push_back(std::move(c));

    return ModelPayload{path.filename().string(), std::move(comps)};
}

void MeshMeditModelHandler::write_components(const ModelLayer& mgr,
    const std::vector<Index>& component_ids,
    const fs::path& path,
    const std::vector<std::any>& args)
{
    if (component_ids.empty()) {
        spdlog::error("MeshMeditModelHandler: write_components called with empty component_ids");
        return;
    }

    // 默认：version 3 + 3D
    int version = 3;
    int dimension = 3;

    if (args.size() >= 1) {
        try {
            version = std::any_cast<int>(args[0]);
        } catch (...) {
            spdlog::warn("MeshMeditModelHandler: invalid version arg, use default 3");
        }
    }
    if (args.size() >= 2) {
        try {
            dimension = std::any_cast<int>(args[1]);
        } catch (...) {
            spdlog::warn("MeshMeditModelHandler: invalid dimension arg, use default 3");
        }
    }

    // 1) 合并多个 component mesh -> 一个导出用“局部 MeshData”
    MeshData merged;
    merged.init(); // 重要：offset 至少有 {0}

    // merged 用局部点编号
    merged.vertex_count_ = 0;

    Index file_vertex_offset = 0; // 文件内顶点偏移（0-based）

    int merged_count = 0;
    for (Index cid : component_ids) {
        const ComponentData* comp = mgr.findComponent(cid);
        if (!comp) {
            spdlog::warn("MeshMeditModelHandler: component {} not found, skip", cid);
            continue;
        }
        if (!comp->mesh) {
            spdlog::warn("MeshMeditModelHandler: component {} has no mesh, skip", cid);
            continue;
        }

        if (appendComponentMeshToMerged(mgr, *comp, merged, file_vertex_offset))
            ++merged_count;
    }

    if (merged_count == 0) {
        spdlog::error("MeshMeditModelHandler: no mesh components to export");
        return;
    }

    merged.vertex_count_ = static_cast<Index>(merged.vertex_positions_.size());

    // 2) 调 libmeshb 写出
    const std::string path_str = path.string();
    const int64_t write_idx = GmfOpenMesh(path_str.c_str(), GmfWrite, version, dimension);
    if (!write_idx) {
        spdlog::error("MeshMeditModelHandler: Failed to create mesh file: {}", path_str);
        return;
    }

    const bool success = LibMeshbIO::write(write_idx, merged);
    GmfCloseMesh(write_idx);

    if (success) {
        spdlog::info("MeshMeditModelHandler: wrote mesh file: {} (version={}, dim={}, components_merged={})",
            path_str, version, dimension, merged_count);
    } else {
        spdlog::error("MeshMeditModelHandler: Failed to write mesh to file: {}", path_str);
    }
}

std::vector<core::ArgType> MeshMeditModelHandler::read_args_type() const
{
    return {};
}

std::vector<core::ArgType> MeshMeditModelHandler::write_args_type() const
{
    return {};
}
}
