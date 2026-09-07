#include "GmshMeshHandler.h"

#include "ArgObject.h"
#include "ComponentData.h"
#include "ComponentOperator.h"
#include "GmshMeshOptions.h"
#include "GeometryData.h"
#include "GeometryRegistry.h"
#include "GeometrySubshapeIndex.h"
#include "IncrementalMeshTools.h"
#include "MeshData.h"
#include "ModelIOSystemBase.h"
#include "ModelLayer.h"
#include "Selection.h"
#include "TempFile.h"

#include <TopAbs_ShapeEnum.hxx>

#include <spdlog/spdlog.h>

#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

using core::ArgType;

namespace {

// Gmsh 网格操作模式。Combo 顺序与枚举值保持一致，默认执行划分。
enum class GmshMeshOperation {
    Mesh = 0,
    Delete = 1
};

// 从 Text 参数读取可选 double；空字符串表示用户不设置，后续使用默认值。
std::optional<double> readOptionalDouble(const std::vector<core::ArgObject>& args, std::size_t index)
{
    if (args.size() <= index)
        return std::nullopt;

    const std::string* text = args[index].get<ArgTypeEnum::Text>();
    if (!text || text->empty())
        return std::nullopt;

    try {
        std::size_t parsed = 0;
        double value = std::stod(*text, &parsed);
        if (parsed != text->size() || !std::isfinite(value))
            throw std::invalid_argument("not a finite double");
        return value;
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
        std::size_t parsed = 0;
        int value = std::stoi(*text, &parsed);
        if (parsed != text->size())
            throw std::invalid_argument("not an integer");
        return value;
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

// 从 Bool 参数读取开关；
bool readBoolArg(const std::vector<core::ArgObject>& args, std::size_t index, bool defaultValue)
{
    if (args.size() <= index)
        return defaultValue;

    const bool* value = args[index].get<ArgTypeEnum::Bool>();
    return value ? *value : defaultValue;
}

// 校验传入 Gmsh 前的数值参数，避免把不合法尺寸或质量值传递给底层库。
bool validateMeshingParameters(const IncrementalMeshTools::GmshMeshParameters& parameters)
{
    if (parameters.targetMeshSize < 0.0
        || parameters.minMeshSize < 0.0
        || parameters.maxMeshSize < 0.0
        || parameters.smoothingSteps < 0
        || parameters.structuredEdgeDivisions < 0
        || parameters.quadMinQuality < 0.0
        || parameters.quadMinQuality > 1.0) {
        spdlog::error("GmshMesh: invalid meshing parameter range");
        return false;
    }

    if (parameters.minMeshSize > 0.0
        && parameters.maxMeshSize > 0.0
        && parameters.minMeshSize > parameters.maxMeshSize) {
        spdlog::error("GmshMesh: minimum mesh size must not exceed maximum mesh size");
        return false;
    }
    return true;
}

} // namespace

std::optional<Index> systems::algo::GmshMeshHandler::resolveComponentId(
    ModelLayer& model_layer,
    Index /*fallback_component_id*/,
    const std::vector<core::ArgObject>& args) const
{
    const auto* selection = args.empty()
        ? nullptr
        : args[0].get<ArgTypeEnum::Selector>();
    if (!selection || !*selection
        || (*selection)->type != ElementEnum::GeometryFace
        || (*selection)->ids.empty()) {
        return std::nullopt;
    }

    std::optional<Index> component_id;
    for (GeomFaceId face_id : (*selection)->ids) {
        const auto owner_id = model_layer.findComponentIdByGeometryShapeId(
            TopAbs_FACE, face_id);
        if (!owner_id) {
            spdlog::error("GmshMesh: component owning geometry face {} was not found", face_id);
            return std::nullopt;
        }
        if (component_id && *component_id != *owner_id) {
            spdlog::error("GmshMesh: selected geometry faces belong to different components");
            return std::nullopt;
        }
        component_id = owner_id;
    }

    return component_id;
}

std::any systems::algo::GmshMeshHandler::execute(
    HandlerContext& context,
    const std::vector<core::ArgObject>& args)
{
    GmshMeshOperation operationMode = GmshMeshOperation::Mesh;
    IncrementalMeshTools::GmshMeshParameters parameters;

    if (args.size() <= 4) {
        // 兼容旧参数顺序：选择面、网格尺寸、操作模式、网格类型。
        parameters.targetMeshSize = readOptionalDouble(args, 1).value_or(0.0);
        if (args.size() >= 3) {
            const std::string* opStr = args[2].get<ArgTypeEnum::Text>();
            if (opStr && !opStr->empty()) {
                try {
                    // 兼容旧接口：1/3 都表示划分，2 表示删除。
                    operationMode = std::stoi(*opStr) == 2
                        ? GmshMeshOperation::Delete
                        : GmshMeshOperation::Mesh;
                } catch (...) {
                    spdlog::warn("GmshMesh: invalid operation mode '{}', using Mesh", *opStr);
                }
            }
        }
        if (args.size() >= 4) {
            const int* value = args[3].get<ArgTypeEnum::Combo>();
            if (value)
                parameters.meshTypeIndex = *value;
        }
    } else {
        operationMode = readComboIndex(args, 1) == static_cast<int>(GmshMeshOperation::Delete)
            ? GmshMeshOperation::Delete
            : GmshMeshOperation::Mesh;
        parameters.targetMeshSize = readOptionalDouble(args, 2).value_or(0.0);
        parameters.minMeshSize = readOptionalDouble(args, 3).value_or(0.0);
        parameters.maxMeshSize = readOptionalDouble(args, 4).value_or(0.0);
        parameters.meshAlgorithm = gmshComboValue(
            kGmshMeshAlgorithmComboValues, readComboIndex(args, 5));
        parameters.meshTypeIndex = readComboIndex(args, 6);
        parameters.algorithmSwitchOnFailure = 0;
        parameters.smoothingSteps = readOptionalInt(args, 7).value_or(0);
        parameters.recombineAlgorithm = gmshComboValue(
            kGmshRecombinationAlgorithmComboValues, readComboIndex(args, 8, 1));
        parameters.quadMinQuality = readOptionalDouble(args, 9).value_or(0.0);
        parameters.structuredEdgeDivisions = readOptionalInt(args, 10).value_or(0);
    }
    bool writeModel = readBoolArg(args, 11, true);

    if (operationMode == GmshMeshOperation::Mesh
        && !validateMeshingParameters(parameters)) {
        return {};
    }

    const ComponentData& comp = context.cur_component.component();
    ModelLayer& modelLayer = context.cur_component.manager();
    GeometryData* geometry = comp.geometry.get();
    if (!geometry || !geometry->rootShape) {
        spdlog::error("GmshMesh: current component {} has no geometry",
            context.cur_component.componentId());
        return {};
    }

    geometry->ensureIndexBuilt(modelLayer.geomRegistry());
    const auto* selection = args.empty()
        ? nullptr
        : args[0].get<ArgTypeEnum::Selector>();
    if (!selection || !*selection
        || (*selection)->type != ElementEnum::GeometryFace
        || (*selection)->ids.empty()) {
        spdlog::error("GmshMesh: select at least one geometry face");
        return {};
    }

    for (GeomFaceId faceId : (*selection)->ids) {
        const TopoDS_Shape* selectedFace = modelLayer.geomRegistry().getFace(faceId);
        const int localFaceId = selectedFace
            ? geometry->index.type_maps[
                  GeometrySubshapeIndex::typeIndex(TopAbs_FACE)].FindIndex(*selectedFace)
            : 0;
        if (localFaceId <= 0) {
            spdlog::error("GmshMesh: selected geometry face {} is not in current component", faceId);
            return {};
        }
    }

    // 划分按整组选中面提交：任一面失败时恢复操作前组件，避免留下部分重划分结果。
    std::unique_ptr<ComponentData> meshOperationSnapshot;
    if (operationMode == GmshMeshOperation::Mesh)
        meshOperationSnapshot = context.cur_component.takeSnapshot();
    bool meshOperationChanged = false;
    const auto restoreMeshOperation = [&]() {
        if (meshOperationSnapshot && meshOperationChanged)
            context.cur_component.restoreSnapshot(*meshOperationSnapshot);
    };

    // 划分时获取或创建当前 component 的 MeshData；删除空网格组件时直接结束。
    // 创建经 replaceMesh（gid 纪律内建 + 标脏）；后续逐面循环经 ComponentOperator 可写入口写入
    if (!comp.mesh) {
        if (operationMode == GmshMeshOperation::Delete) {
            spdlog::warn("GmshMesh: current component has no mesh to delete");
            return {};
        }
        spdlog::info("GmshMesh: mesh_data is null, create a new one");
        auto new_mesh = std::make_unique<MeshData>();
        new_mesh->init();
        context.cur_component.replaceMesh(std::move(new_mesh));
        meshOperationChanged = true;
    }

    // 本次操作在通用映射副本上执行，成功路径结束后再统一提交。
    const GeometryMeshMap* component_mapping = context.cur_component.geometryMeshMap();
    GeometryMeshMap working_mapping = component_mapping
        ? *component_mapping
        : GeometryMeshMap {};

    // Gmsh 私有状态只保留共享边坐标及由通用映射重建的临时引用计数。
    auto state_result = IncrementalMeshTools::buildStateFromGeometryMeshMap(
        component_mapping, *context.cur_component.mesh(),
        *geometry, modelLayer.geomRegistry());
    if (!state_result) {
        restoreMeshOperation();
        return {};
    }
    GmshIncrementalMeshState state = std::move(*state_result);

    if (operationMode == GmshMeshOperation::Mesh
        && parameters.targetMeshSize <= 0.0) {
        parameters.targetMeshSize = IncrementalMeshTools::estimateMeshSize(*geometry);
    }

    std::size_t successCount = 0;
    std::size_t failedCount = 0;
    std::size_t triangle_count = 0;
    std::size_t quadrangle_count = 0;
    bool state_changed = false;

    if (operationMode == GmshMeshOperation::Mesh) {
        // 先统一移除所选面的旧网格，使选中相邻面之间的共享边可以共同重新离散。
        for (GeomFaceId faceId : (*selection)->ids) {
            if (working_mapping.geometry_face_to_mesh_topology.find(faceId)
                == working_mapping.geometry_face_to_mesh_topology.end()) {
                continue;
            }
            if (!IncrementalMeshTools::deleteFaceMesh(
                    *geometry, state, working_mapping,
                    context.cur_component, faceId)) {
                spdlog::warn("GmshMesh: failed to prepare face {} for remeshing", faceId);
                restoreMeshOperation();
                return {};
            }
            meshOperationChanged = true;
        }

        for (GeomFaceId faceId : (*selection)->ids) {
            auto result = IncrementalMeshTools::meshSingleFace(
                *geometry, state, working_mapping, context.cur_component,
                faceId, parameters.targetMeshSize, parameters);
            if (!result.success) {
                spdlog::warn("GmshMesh: face {} meshing failed", faceId);
                restoreMeshOperation();
                spdlog::info("GmshMesh: selected faces restored after meshing failure");
                return {};
            }
            meshOperationChanged = true;
            triangle_count += result.triangle_count;
            quadrangle_count += result.quadrangle_count;
            ++successCount;
        }
        state_changed = successCount > 0;
    } else {
        for (GeomFaceId faceId : (*selection)->ids) {
            if (!IncrementalMeshTools::deleteFaceMesh(
                    *geometry, state, working_mapping,
                    context.cur_component, faceId)) {
                spdlog::warn("GmshMesh: delete face {} failed", faceId);
                ++failedCount;
                continue;
            }
            state_changed = true;
            ++successCount;
        }
    }

    if (state_changed
        && !IncrementalMeshTools::storeStateToGeometryMeshMap(
            state, working_mapping, context.cur_component)) {
        spdlog::error("GmshMesh: failed to update geometry-mesh mapping");
        restoreMeshOperation();
        return {};
    }

    spdlog::info("GmshMesh: {} selected faces processed: {} succeeded, {} failed",
        (*selection)->ids.size(), successCount, failedCount);
    if (operationMode == GmshMeshOperation::Mesh) {
        spdlog::info(
            "GmshMesh: mesh summary: {} elements ({} triangles, {} quadrangles)",
            triangle_count + quadrangle_count,
            triangle_count,
            quadrangle_count);
    }

    if (writeModel && successCount > 0) {
        std::string meshOut = core::TempFile::instance().path().string() + "_total_mesh.obj";
        context.io_system.writeComponents(
            { context.cur_component.componentId() },
            meshOut,
            "Wavefront .obj file",
            {});
        context.io_system.read(meshOut, "Wavefront .obj file", {});
    }

    return {};
}

std::vector<ArgType> systems::algo::GmshMeshHandler::args_type() const
{
    return {
        ArgType { ArgTypeEnum::Selector, "选择几何面", "GeometryFace" },
        ArgType { ArgTypeEnum::Combo, "操作模式", "划分,删除" },
        ArgType { ArgTypeEnum::Text, "目标网格尺寸(留空自动)", "" },
        ArgType { ArgTypeEnum::Text, "最小网格尺寸(留空默认)", "" },
        ArgType { ArgTypeEnum::Text, "最大网格尺寸(留空默认)", "" },
        ArgType { ArgTypeEnum::Combo, "二维网格算法", kGmshMeshAlgorithmComboText },
        ArgType { ArgTypeEnum::Combo, "网格类型", kGmshSurfaceMeshTypeComboText },
        ArgType { ArgTypeEnum::Text, "平滑次数(留空默认)", "" },
        ArgType { ArgTypeEnum::Combo, "四边形重组算法", kGmshRecombinationAlgorithmComboText },
        ArgType { ArgTypeEnum::Text, "四边形最低质量(0~1，留空默认)", "" },
        ArgType { ArgTypeEnum::Text, "结构化网格边划分数(留空自动)", "" },
        ArgType { ArgTypeEnum::Bool, "是否导出网格", "false" }
    };
}
