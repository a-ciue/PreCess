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
    auto mesh_data = ObjMeshIO::loadFromFile(path);

    auto c = std::make_unique<ComponentData>();
    c->id = -1;
    c->name = "Comp_0";
    c->mesh = std::move(mesh_data);

    ComponentDatas comps;
    comps.push_back(std::move(c));

    return ModelPayload{path.filename().string(), std::move(comps)};
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

        const Index cnt = m.vertex_count_;
        if (cnt <= 0) {
            spdlog::warn("OBJModelHandler: component {} has no vertices, skip", cid);
            continue;
        }

        std::unordered_map<Index, Index> global_to_local;
        for (Index i = 0; i < cnt; ++i) {
            global_to_local[m.local_to_global_[i]] = i;
        }

        // object name
        ofs << "o " << (comp->name.empty() ? ("component_" + std::to_string(cid)) : comp->name) << "\n";

        // vertices
        for (Index i = 0; i < cnt; ++i) {
            const Index gid = m.local_to_global_[i];
            const auto& p = gp[(size_t)gid];
            ofs << "v " << p[0] << " " << p[1] << " " << p[2] << "\n";
        }

        bool component_ok = true;
        if (m.face_vertices_offset_.size() >= 2) {
            const Index nFaces = static_cast<Index>(m.face_vertices_offset_.size() - 1);
            for (Index f = 0; f < nFaces && component_ok; ++f) {
                const Index a = m.face_vertices_offset_[static_cast<size_t>(f)];
                const Index b = m.face_vertices_offset_[static_cast<size_t>(f + 1)];
                if (a < 0 || b < a || b > static_cast<Index>(m.face_vertices_.size()))
                    continue;

                ofs << "f";
                for (Index k = a; k < b; ++k) {
                    const Index gid = m.face_vertices_[static_cast<size_t>(k)];
                    auto it = global_to_local.find(gid);
                    if (it == global_to_local.end()) {
                        spdlog::error("OBJModelHandler: face references vertex not in component, cid={}, gid={}",
                            cid, gid);
                        ofs << "\n";
                        component_ok = false;
                        break;
                    }
                    ofs << " " << (v_offset_1based + it->second);
                }
                if (component_ok) {
                    ofs << "\n";
                }
            }
        }

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
