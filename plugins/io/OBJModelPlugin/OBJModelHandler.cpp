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
std::unique_ptr<ModelData> OBJModelHandler::read_model(const fs::path& path, const std::vector<std::any>& args)
{
    // MeshData
    auto mesh_data = ObjMeshIO::loadFromFile(path);

    // ModelData
    auto model_data = std::make_unique<ModelData>(std::move(mesh_data));
    model_data->model_name_ = path.filename().string();

    return model_data; // 技巧：RVO
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

    const auto& gp = mgr.globalPoints();

    // OBJ vertex indices are 1-based and file-global, need accumulated offset
    Index v_offset_1based = 1;

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

        const MeshData& m = *comp->mesh;

        const Index base = m.global_point_base_;
        const Index cnt = m.vertex_count_;

        if (base < 0 || cnt <= 0) {
            spdlog::warn("OBJModelHandler: component {} has invalid mesh point range (base={}, cnt={}), skip",
                cid, base, cnt);
            continue;
        }
        if (base + cnt > (Index)gp.size()) {
            spdlog::error("OBJModelHandler: globalPoints out of range for component {} (base={}, cnt={}, gp={})",
                cid, base, cnt, gp.size());
            continue;
        }

        // object name
        ofs << "o " << (comp->name.empty() ? ("component_" + std::to_string(cid)) : comp->name) << "\n";

        // vertices
        for (Index i = 0; i < cnt; ++i) {
            const auto& p = gp[(size_t)(base + i)];
            ofs << "v " << p[0] << " " << p[1] << " " << p[2] << "\n";
        }

        // faces (support polygon via offsets)
        if (m.face_vertices_offset_.size() >= 2) {
            const Index nFaces = (Index)m.face_vertices_offset_.size() - 1;
            for (Index f = 0; f < nFaces; ++f) {
                const Index a = m.face_vertices_offset_[(size_t)f];
                const Index b = m.face_vertices_offset_[(size_t)f + 1];
                if (a < 0 || b < a || b > (Index)m.face_vertices_.size())
                    continue;

                ofs << "f";
                for (Index k = a; k < b; ++k) {
                    const Index gid = m.face_vertices_[(size_t)k]; // global point id
                    const Index local = gid - base; // to local 0..cnt-1
                    if (local < 0 || local >= cnt) {
                        spdlog::error("OBJModelHandler: face references vertex out of component range, cid={}, gid={}, base={}, cnt={}",
                            cid, gid, base, cnt);
                        ofs << "\n";
                        goto next_component; // break face loop and proceed
                    }
                    ofs << " " << (v_offset_1based + local);
                }
                ofs << "\n";
            }
        }

    next_component:
        v_offset_1based += cnt;
    }
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
