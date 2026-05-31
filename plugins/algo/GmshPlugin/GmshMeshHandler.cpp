#include "GmshMeshHandler.h"

#include "ArgObject.h"
#include "ComponentData.h"
#include "ComponentOperator.h"
#include "GeometryData.h"
#include "IncrementalMeshContext.h"
#include "IncrementalMeshTools.h"
#include "MeshData.h"
#include "ModelIOSystemBase.h"
#include "ModelLayer.h"
#include "TempFile.h"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <string>

using core::ArgType;

std::any systems::algo::GmshMeshHandler::execute(
    HandlerContext& context,
    const std::vector<core::ArgObject>& args)
{
    int faceIndex = -1;
    double meshSize = 0.0;
    int operationMode = 1;

    if (args.size() >= 1) {
        const std::string* idxStr = args[0].get<ArgTypeEnum::Text>();
        if (idxStr && !idxStr->empty()) {
            try {
                faceIndex = std::stoi(*idxStr);
            } catch (...) {
                spdlog::warn("GmshMesh: invalid face index '{}'", *idxStr);
            }
        }
    }

    if (args.size() >= 2) {
        const std::string* sizeStr = args[1].get<ArgTypeEnum::Text>();
        if (sizeStr && !sizeStr->empty()) {
            try {
                meshSize = std::stod(*sizeStr);
            } catch (...) {
                spdlog::warn("GmshMesh: invalid size '{}'", *sizeStr);
            }
        }
    }

    if (args.size() >= 3) {
        const std::string* opStr = args[2].get<ArgTypeEnum::Text>();
        if (opStr && !opStr->empty()) {
            try {
                operationMode = std::stoi(*opStr);
            } catch (...) {
                spdlog::warn("GmshMesh: invalid operation mode '{}', using default 1 (Mesh)", *opStr);
            }
        }
    }

    if (faceIndex < 0) {
        spdlog::error("GmshMesh: need face id");
        return {};
    }

    ComponentData& comp = context.cur_component.component();
    ModelLayer& modelLayer = context.cur_component.manager();
    removeExpiredStates(modelLayer);

    GeometryData* geometry = comp.geometry.get();
    if (!geometry || !geometry->rootShape) {
        spdlog::error("GmshMesh: current component {} has no geometry",
            context.cur_component.componentId());
        return {};
    }

    // 获取或创建当前 component 的 MeshData，Gmsh 生成结果直接写回该 component。
    MeshData* meshData = comp.mesh.get();
    if (!meshData) {
        spdlog::info("GmshMesh: mesh_data is null, create a new one");
        comp.mesh = std::make_unique<MeshData>();
        comp.mesh->init();
        meshData = comp.mesh.get();
    }

    GmshIncrementalMeshState& state = component_states_[context.cur_component.componentId()];

    // 首次执行时建立 OCC 面/边索引，后续单面划分复用该上下文。
    if (!state.meshContext) {
        spdlog::info("GmshMesh: init occId...");
        state.meshContext = std::make_unique<IncrementalMeshContext>(*geometry->rootShape);
        spdlog::info("GmshMesh: {} face, {} global edge",
            state.meshContext->faceCount(),
            state.meshContext->globalEdgeCount());
    }

    std::size_t totalFaces = state.meshContext->faceCount();
    if (static_cast<std::size_t>(faceIndex) >= totalFaces) {
        spdlog::error("GmshMesh: face id {} out of range (total {} face)",
            faceIndex, totalFaces);
        return {};
    }

    if (meshSize <= 0.0)
        meshSize = IncrementalMeshTools::estimateMeshSize(*geometry);

    SingleFaceMeshResult result;

    if (operationMode == 2) {
        spdlog::info("GmshMesh: delete mesh for face {}", faceIndex);
        if (IncrementalMeshTools::deleteFaceMesh(*meshData, state, modelLayer, static_cast<std::size_t>(faceIndex)))
            spdlog::info("GmshMesh: delete face {} success", faceIndex);
    } else if (operationMode == 3) {
        spdlog::info("GmshMesh: remesh face {} (size={:.4f})", faceIndex, meshSize);
        result = IncrementalMeshTools::remeshSingleFace(
            *meshData, *geometry, state, modelLayer, static_cast<std::size_t>(faceIndex), meshSize);
    } else {
        spdlog::info("GmshMesh: mesh face {} (size={:.4f})", faceIndex, meshSize);
        result = IncrementalMeshTools::meshSingleFace(
            *meshData, *geometry, state, modelLayer, static_cast<std::size_t>(faceIndex), meshSize);
    }

    if (operationMode != 2) {
        if (!result.success) {
            spdlog::warn("GmshMesh: face {} meshing failed", faceIndex);
            return {};
        }

        spdlog::info("GmshMesh: face {} finish, {} nodes, {} cells, cached {} edge",
            faceIndex,
            result.vertices.size(),
            result.face_vertices_offset.size() - 1,
            state.meshedEdgeRefCounts.size());

        //std::string faceOut = core::TempFile::instance().path().string() + "_single_face_" + std::to_string(faceIndex) + ".obj";
        //if (!IncrementalMeshTools::writeSingleFaceObj(result, faceOut)) {
        //    spdlog::error("GmshMesh: cant save single face");
        //    return {};
        //}
        //context.io_system.read(faceOut, "Wavefront .obj file", {});
    }

    std::string meshOut = core::TempFile::instance().path().string() + "_total_mesh_" + std::to_string(faceIndex) + ".obj";
    if (!IncrementalMeshTools::writeMeshObj(*meshData, state, meshOut)) {
        spdlog::error("GmshMesh: cant save meshdata");
        return {};
    }
    context.io_system.read(meshOut, "Wavefront .obj file", {});
    context.cur_component.notifyChanged();

    return {};
}

void systems::algo::GmshMeshHandler::removeExpiredStates(const ModelLayer& model_layer)
{
    for (auto it = component_states_.begin(); it != component_states_.end();) {
        if (!model_layer.findComponent(it->first)) {
            it = component_states_.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<ArgType> systems::algo::GmshMeshHandler::args_type() const
{
    return {
        ArgType { ArgTypeEnum::Text, "面索引(0开始)", "" },
        ArgType { ArgTypeEnum::Text, "网格尺寸(留空自动)", "" },
        ArgType { ArgTypeEnum::Text, "1: mesh 2: delete 3: remesh", "" }
    };
}
