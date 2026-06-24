#include "GmshMeshHandler.h"

#include "ArgObject.h"
#include "ComponentData.h"
#include "ComponentOperator.h"
#include "GmshMeshOptions.h"
#include "GeometryData.h"
#include "IncrementalMeshTools.h"
#include "MeshData.h"
#include "ModelIOSystemBase.h"
#include "ModelLayer.h"
#include "TempFile.h"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <optional>
#include <string>

using core::ArgType;

namespace {

// 从 Text 参数读取可选 double；空字符串表示用户不设置，后续使用默认值。
std::optional<double> readOptionalDouble(const std::vector<core::ArgObject>& args, std::size_t index)
{
    if (args.size() <= index)
        return std::nullopt;

    const std::string* text = args[index].get<ArgTypeEnum::Text>();
    if (!text || text->empty())
        return std::nullopt;

    try {
        return std::stod(*text);
    } catch (...) {
        spdlog::warn("GmshMesh: invalid double parameter {} = '{}'", index, *text);
        return std::nullopt;
    }
}

// 从 Text 参数读取可选 int；空字符串表示用户不设置。
std::optional<int> readOptionalInt(const std::vector<core::ArgObject>& args, std::size_t index)
{
    if (args.size() <= index)
        return std::nullopt;

    const std::string* text = args[index].get<ArgTypeEnum::Text>();
    if (!text || text->empty())
        return std::nullopt;

    try {
        return std::stoi(*text);
    } catch (...) {
        spdlog::warn("GmshMesh: invalid int parameter {} = '{}'", index, *text);
        return std::nullopt;
    }
}

// Combo 参数没有“空值”，所以第一个选项统一作为“默认”。
int readComboIndex(const std::vector<core::ArgObject>& args, std::size_t index, int defaultValue = 0)
{
    if (args.size() <= index)
        return defaultValue;

    const int* value = args[index].get<ArgTypeEnum::Combo>();
    return value ? *value : defaultValue;
}

} // namespace

std::any systems::algo::GmshMeshHandler::execute(
    HandlerContext& context,
    const std::vector<core::ArgObject>& args)
{
    int faceIndex = -1;
    int operationMode = 1;
    IncrementalMeshTools::GmshMeshParameters parameters;

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

    if (args.size() <= 4) {
        // 兼容旧参数顺序：面索引、网格尺寸、操作模式、网格类型。
        parameters.targetMeshSize = readOptionalDouble(args, 1).value_or(0.0);
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
        if (args.size() >= 4) {
            const int* value = args[3].get<ArgTypeEnum::Combo>();
            if (value)
                parameters.meshTypeIndex = *value;
        }
    } else {
        operationMode = readComboIndex(args, 1) + 1;
        parameters.targetMeshSize = readOptionalDouble(args, 2).value_or(0.0);
        parameters.minMeshSize = readOptionalDouble(args, 3).value_or(0.0);
        parameters.maxMeshSize = readOptionalDouble(args, 4).value_or(0.0);
        parameters.meshAlgorithm = gmshComboValue(
            kGmshMeshAlgorithmComboValues, readComboIndex(args, 5));
        parameters.meshTypeIndex = readComboIndex(args, 6);
        parameters.algorithmSwitchOnFailure = 0;
        parameters.smoothingSteps = readOptionalInt(args, 7).value_or(0);
        parameters.recombineAlgorithm = gmshComboValue(
            kGmshRecombinationAlgorithmComboValues, readComboIndex(args, 8));
        parameters.recombineAngle = readOptionalDouble(args, 9).value_or(45.0);
        parameters.quadMinQuality = readOptionalDouble(args, 10).value_or(0.0);
        parameters.recombineOptimizeTopology = readOptionalInt(args, 11).value_or(0);
        parameters.structuredEdgeDivisions = readOptionalInt(args, 12).value_or(0);
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

    geometry->ensureCadIndexBuilt(modelLayer.geomRegistry());
    std::size_t totalFaces = IncrementalMeshTools::faceCount(*geometry);
    if (static_cast<std::size_t>(faceIndex) >= totalFaces) {
        spdlog::error("GmshMesh: face id {} out of range (total {} face)",
            faceIndex, totalFaces);
        return {};
    }

    if (parameters.targetMeshSize <= 0.0)
        parameters.targetMeshSize = IncrementalMeshTools::estimateMeshSize(*geometry);

    std::size_t faceKey = static_cast<std::size_t>(faceIndex);
    SingleFaceMeshResult result;

    if (operationMode == 1) {
        if (state.meshedFacesCache.find(faceKey) != state.meshedFacesCache.end()) {
            spdlog::info("GmshMesh: face {} already meshed, skip mesh mode; use remesh mode to rebuild", faceIndex);
            return {};
        }
        spdlog::info("GmshMesh: mesh face {} (size={:.4f})", faceIndex, parameters.targetMeshSize);
        result = IncrementalMeshTools::meshSingleFace(
            *meshData, *geometry, state, modelLayer, faceKey, parameters.targetMeshSize, parameters);
    } else if (operationMode == 2) {
        spdlog::info("GmshMesh: delete mesh for face {}", faceIndex);
        if (IncrementalMeshTools::deleteFaceMesh(
                *meshData, *geometry, state, modelLayer, faceKey))
            spdlog::info("GmshMesh: delete face {} success", faceIndex);
    } else if (operationMode == 3) {
        spdlog::info("GmshMesh: remesh face {} (size={:.4f})", faceIndex, parameters.targetMeshSize);
        result = IncrementalMeshTools::remeshSingleFace(
            *meshData, *geometry, state, modelLayer, faceKey, parameters.targetMeshSize, parameters);
    } else {
        spdlog::warn("GmshMesh: unknown operation mode {}, skip", operationMode);
        return {};
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

        // std::string faceOut = core::TempFile::instance().path().string() + "_single_face_" + std::to_string(faceIndex) + ".obj";
        // if (!IncrementalMeshTools::writeSingleFaceObj(result, faceOut)) {
        //     spdlog::error("GmshMesh: cant save single face");
        //     return {};
        // }
        // context.io_system.read(faceOut, "Wavefront .obj file", {});
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
        ArgType { ArgTypeEnum::Text, "CAD面索引(0开始)", "" },
        ArgType { ArgTypeEnum::Combo, "操作模式", "划分,删除,重划分" },
        ArgType { ArgTypeEnum::Text, "目标网格尺寸(留空自动)", "" },
        ArgType { ArgTypeEnum::Text, "最小网格尺寸(留空默认)", "" },
        ArgType { ArgTypeEnum::Text, "最大网格尺寸(留空默认)", "" },
        ArgType { ArgTypeEnum::Combo, "二维网格算法", kGmshMeshAlgorithmComboText },
        ArgType { ArgTypeEnum::Combo, "网格类型", kGmshSurfaceMeshTypeComboText },
        ArgType { ArgTypeEnum::Text, "平滑次数(留空默认)", "" },
        ArgType { ArgTypeEnum::Combo, "四边形重组算法", kGmshRecombinationAlgorithmComboText },
        ArgType { ArgTypeEnum::Text, "重组角度阈值(默认45)", "" },
        ArgType { ArgTypeEnum::Text, "四边形最低质量(0~1，留空默认)", "" },
        ArgType { ArgTypeEnum::Text, "拓扑优化次数(留空默认)", "" },
        ArgType { ArgTypeEnum::Text, "结构化网格边划分数(留空自动)", "" }
    };
}
