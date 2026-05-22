#include "GmshMeshHandler.h"
#include "ModelOperatorBase.h"
#include "ArgObject.h"
#include "IncrementalMeshContext.h"
#include "IncrementalMeshTools.h"
#include "ModelData.h"
#include "ModelIOSystemBase.h"
#include "SplineData.h"
#include "ModelOperator.h" 
#include "MeshData.h"
#include <spdlog/spdlog.h>

#include <filesystem>
#include <string>
#include <TempFile.h>

using core::ArgType;

std::any systems::algo::GmshMeshHandler::execute(
    HandlerContext& context,
    const std::vector<core::ArgObject>& args)
{
    // 解析参数：面索引 + 网格尺寸 + 操作类型
    int faceIndex = -1;
    double meshSize = 0.0;
    int operationMode = 1; // 默认 1：网格划分

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

    // 获取 SplineData
    if (context.cur_model.getType() != ModelData::Type::Spline) {
        spdlog::error("GmshMesh: need Spline model");
        return {};
    }
    ModelOperator* op = dynamic_cast<ModelOperator*>(&context.cur_model);
    if (!op) {
        spdlog::error("GmshMesh: cant get ModelOperator");
        return {};
    }

    SplineData* sp = op->data().asSplineData();
    if (!sp || !sp->rootShape) {
        spdlog::error("GmshMesh: SplineData is illegal");
        return {};
    }
    
    // 获取 MeshData
    MeshData* mesh_data = op->data().getMeshData();
    if (!mesh_data) {
        spdlog::info("GmshMesh: mesh_data is null, create a new one");
        auto new_mesh = std::make_unique<MeshData>();
        new_mesh->init();
        op->data().setMeshData(std::move(new_mesh));
        mesh_data = op->data().getMeshData();
        if (!mesh_data) {
            spdlog::error("GmshMesh: failed to create mesh_data");
            return {};
        }
    }
    
    // 首次调用：初始化拓扑索引
    if (!sp->meshContext) {
        spdlog::info("GmshMesh: init occId...");
        sp->meshContext = std::make_unique<IncrementalMeshContext>(*sp->rootShape);
        spdlog::info("GmshMesh: {} face, {} global edge",
            sp->meshContext->faceCount(),
            sp->meshContext->globalEdgeCount());
    }

    // 检查面索引范围 
    std::size_t totalFaces = sp->meshContext->faceCount();
    if (static_cast<std::size_t>(faceIndex) >= totalFaces) {
        spdlog::error("GmshMesh: face id {} out of range (total {} face)",
            faceIndex, totalFaces);
        return {};
    }

    if (meshSize <= 0.0)
        meshSize = IncrementalMeshTools::estimateMeshSize(*sp);

    SingleFaceMeshResult result;

    if (operationMode == 2) {
        // 删除网格
        spdlog::info("GmshMesh: delete mesh for face {}", faceIndex);
        if (IncrementalMeshTools::deleteFaceMesh(*mesh_data, *sp, static_cast<std::size_t>(faceIndex)))
            spdlog::info("GmshMesh: delete face  {} sucess", faceIndex);
    }
    else if (operationMode == 3) {
        // 重划分网格
        spdlog::info("GmshMesh: remesh face {} (size={:.4f})", faceIndex, meshSize);
        result = IncrementalMeshTools::remeshSingleFace(
            *mesh_data, *sp, static_cast<std::size_t>(faceIndex), meshSize);
    }
    else {
        // 划分网格 (默认)
        spdlog::info("GmshMesh: mesh face {} (size={:.4f})", faceIndex, meshSize);
        result = IncrementalMeshTools::meshSingleFace(
            *mesh_data, *sp, static_cast<std::size_t>(faceIndex), meshSize);
    }

    if (operationMode != 2) {
        if (!result.success) {
            spdlog::warn("GmshMesh: face {} meshing failed", faceIndex);
            return {};
        }

        spdlog::info("GmshMesh:face {} finish , {} nodes, {} cells , cached {} edge",
            faceIndex,
            result.vertices.size(),
            result.face_vertices_offset.size() - 1,
            sp->meshedEdgeRefCounts.size());

        // 单面输出为obj
        std::string face_out = core::TempFile::instance().path().string() +"_single_face_" + std::to_string(faceIndex) + ".obj";
        if (!IncrementalMeshTools::writeSingleFaceObj(result, face_out)) {
            spdlog::error("GmshMesh: cant save single face ");
            return {};
        }
        context.io_system.read(face_out, "Wavefront .obj file", {});
    }

    // 将总的mesh_data写出为obj
    std::string mesh_out = core::TempFile::instance().path().string() + "_total_mesh_" + std::to_string(faceIndex) + ".obj";
    if (!IncrementalMeshTools::writeMeshObj(*mesh_data, mesh_out)) {
        spdlog::error("GmshMesh: cant save meshdata ");
        return {};
    }
    context.io_system.read(mesh_out, "Wavefront .obj file", {});
    
    return {};
}

std::vector<ArgType> systems::algo::GmshMeshHandler::args_type() const
{
    return {
        ArgType { ArgTypeEnum::Text, "面索引(0开始)", "" },
        ArgType { ArgTypeEnum::Text, "网格尺寸(留空自动)", "" },
        ArgType { ArgTypeEnum::Text, "1：mesh 2：delete 3：remesh", "" }
    };
}