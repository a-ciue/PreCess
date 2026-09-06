/**
 * @file OffModelHandler.cpp
 * @brief OFF 模型文件处理器实现
 */
#include "OffModelHandler.h"

#include "ArgType.h"
#include "ComponentData.h"
#include "MeshData.h"
#include "ModelData.h"
#include "ModelLayer.h"
#include "OffMeshIO.h"

#include <spdlog/spdlog.h>

namespace {
/**
 * @brief 把一个组件的网格追加到 merged，多组件导出时按点偏移拼成一个网格
 *
 * MeshData 自包含、连通性存组件内局部点索引，追加时统一加 vertex_offset
 * 换算为文件内点序号（OFF 没有组件概念，导出结果是一个整体网格）。
 * @param vertex_offset 入参为当前文件内点偏移，出参累加本组件的点数
 */
bool appendComponentMesh(const ComponentData& component, MeshData& merged, Index& vertex_offset)
{
    const MeshData* source = component.mesh.get();
    if (!source) {
        return false;
    }

    const Index point_count = static_cast<Index>(source->vertex_positions_.size());
    if (point_count <= 0) {
        spdlog::warn("OffModelHandler: component {} has no vertices, skip", component.id);
        return false;
    }

    merged.vertex_positions_.insert(merged.vertex_positions_.end(),
        source->vertex_positions_.begin(), source->vertex_positions_.end());

    // 面：跳过越界的脏面；退化面与点索引越界的面由 OffMeshIO::write 统一过滤
    if (source->face_vertices_offset_.size() >= 2) {
        const Index face_count = static_cast<Index>(source->face_vertices_offset_.size() - 1);
        const Index corner_count = static_cast<Index>(source->face_vertices_.size());

        for (Index f = 0; f < face_count; ++f) {
            const Index begin = source->face_vertices_offset_[static_cast<size_t>(f)];
            const Index end = source->face_vertices_offset_[static_cast<size_t>(f) + 1];
            if (begin < 0 || end < begin || end > corner_count) {
                continue;
            }

            for (Index c = begin; c < end; ++c) {
                merged.face_vertices_.push_back(vertex_offset + source->face_vertices_[static_cast<size_t>(c)]);
            }
            merged.face_vertices_offset_.push_back(static_cast<Index>(merged.face_vertices_.size()));
        }
    }

    vertex_offset += point_count;
    return true;
}
} // namespace

namespace systems::io {
using core::ArgType;

std::optional<ModelPayload> OffModelHandler::read_model(const fs::path& path, const std::vector<std::any>& args)
{
    // OFF 只承载点与面，读入结果是一个网格组件
    auto mesh = std::make_unique<MeshData>();
    if (!OffMeshIO::read(path, *mesh)) {
        spdlog::error("OffModelHandler: failed to read OFF file: {}", path.string());
        return std::nullopt;
    }

    auto component = std::make_unique<ComponentData>();
    component->id = -1; // 组件 id 由模型层入池时分配
    component->name = "Comp_0"; // OFF 无分组概念，整个文件作为一个组件
    component->mesh = std::move(mesh);

    ComponentDatas components;
    components.push_back(std::move(component));

    return ModelPayload { path.filename().u8string(), std::move(components) };
}

void OffModelHandler::write_components(const ModelLayer& mgr,
    const std::vector<Index>& component_ids,
    const fs::path& path,
    const std::vector<std::any>& /*args*/)
{
    if (component_ids.empty()) {
        spdlog::error("OffModelHandler: write_components called with empty component_ids");
        return;
    }

    MeshData merged;
    merged.init(); // 面 offsets 至少有 {0}

    Index vertex_offset = 0;
    int merged_count = 0;
    for (Index cid : component_ids) {
        const ComponentData* component = mgr.findComponent(cid);
        if (!component) {
            spdlog::warn("OffModelHandler: component {} not found, skip", cid);
            continue;
        }
        if (!component->mesh) {
            spdlog::warn("OffModelHandler: component {} has no mesh, skip", cid);
            continue;
        }

        if (appendComponentMesh(*component, merged, vertex_offset)) {
            ++merged_count;
        }
    }

    if (merged_count == 0) {
        spdlog::error("OffModelHandler: no mesh component to export");
        return;
    }

    merged.vertex_count_ = static_cast<Index>(merged.vertex_positions_.size());
    if (OffMeshIO::write(path, merged)) {
        spdlog::info("OffModelHandler: wrote OFF file: {} (components_merged={})", path.string(), merged_count);
    } else {
        spdlog::error("OffModelHandler: failed to write OFF file: {}", path.string());
    }
}

std::vector<ArgType> OffModelHandler::read_args_type() const
{
    return {};
}

std::vector<ArgType> OffModelHandler::write_args_type() const
{
    return {};
}
}
