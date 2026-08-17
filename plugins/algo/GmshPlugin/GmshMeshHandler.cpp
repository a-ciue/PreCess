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
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

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

/**
 * @brief 取得一个几何面包含的去重全局边 id
 */
std::vector<GeomEdgeId> getFaceEdgeIds(
    const TopoDS_Face& face,
    const GeometryData& geometry)
{
    const auto& edge_map =
        geometry.index.type_maps[GeometrySubshapeIndex::typeIndex(TopAbs_EDGE)];

    std::vector<GeomEdgeId> edge_ids;
    for (TopExp_Explorer explorer(face, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        const int local_edge_id = edge_map.FindIndex(explorer.Current());
        const GeomEdgeId edge_id = geometry.index.edgeGlobalId(local_edge_id);
        if (edge_id != kInvalidGeomEdgeId
            && std::find(edge_ids.begin(), edge_ids.end(), edge_id) == edge_ids.end()) {
            edge_ids.push_back(edge_id);
        }
    }
    return edge_ids;
}

/**
 * @brief 从可快照的通用 Geometry↔Mesh 映射重建本次操作使用的 Gmsh 临时状态
 *
 * 边参数坐标不持久化；Gmsh 注入共享边时会按节点坐标重新投影计算参数。
 */
std::optional<GmshIncrementalMeshState> buildGmshState(
    const GeometryMeshMap* mapping,
    const MeshData& mesh,
    const GeometryData& geometry,
    const GeometryRegistry& registry)
{
    GmshIncrementalMeshState state;
    if (!mapping)
        return state;

    for (const auto& [edge_id, point_ids] : mapping->geometry_edge_to_mesh_point_ids) {
        MeshedEdgeData edge_data;
        edge_data.coords.reserve(point_ids.size() * 3);
        for (Index point_id : point_ids) {
            if (point_id < 0
                || point_id >= static_cast<Index>(mesh.vertex_positions_.size())) {
                spdlog::error(
                    "GmshMesh: geometry edge {} references invalid mesh point {}",
                    edge_id, point_id);
                return std::nullopt;
            }
            const auto& position = mesh.vertex_positions_[static_cast<std::size_t>(point_id)];
            edge_data.coords.insert(
                edge_data.coords.end(), position.begin(), position.end());
        }
        state.meshedEdgesCache.emplace(edge_id, std::move(edge_data));
    }

    for (const auto& [face_id, topology] : mapping->geometry_face_to_mesh_topology) {
        if (topology.face_vertices_offset.size() < 2
            || topology.face_vertices_offset.front() != 0
            || topology.face_vertices_offset.back()
                != static_cast<Index>(topology.face_vertices.size())) {
            spdlog::error("GmshMesh: geometry face {} has invalid mesh topology", face_id);
            return std::nullopt;
        }

        SingleFaceMeshResult result;
        result.global_face_vertices = topology.face_vertices;
        result.face_vertices.resize(topology.face_vertices.size());
        result.face_vertices_offset.reserve(topology.face_vertices_offset.size());
        for (Index offset : topology.face_vertices_offset) {
            if (offset < 0) {
                spdlog::error("GmshMesh: geometry face {} has negative topology offset", face_id);
                return std::nullopt;
            }
            result.face_vertices_offset.push_back(static_cast<std::size_t>(offset));
        }
        result.success = true;
        state.meshedFacesCache.emplace(face_id, std::move(result));
    }

    // 引用计数是 Gmsh 的派生状态，由已映射面及其几何边关系重新计算。
    for (const auto& [face_id, unused] : state.meshedFacesCache) {
        (void)unused;
        const TopoDS_Shape* shape = registry.getFace(face_id);
        if (!shape)
            continue;
        for (GeomEdgeId edge_id : getFaceEdgeIds(TopoDS::Face(*shape), geometry)) {
            if (state.meshedEdgesCache.find(edge_id) != state.meshedEdgesCache.end())
                ++state.meshedEdgeRefCounts[edge_id];
        }
    }
    return state;
}

/**
 * @brief 按 MeshData 合并节点时的相同量化规则查找组件内局部点 id
 *
 * 使用相同键空间可避免距离搜索把两个不同量化格中的相邻节点错误合并。
 */
class MeshPointLookup {
public:
    explicit MeshPointLookup(const MeshData& mesh)
    {
        for (Index point_id = 0;
             point_id < static_cast<Index>(mesh.vertex_positions_.size()); ++point_id) {
            const auto& position = mesh.vertex_positions_[static_cast<std::size_t>(point_id)];
            points_[quantize(position)] = point_id;
        }
    }

    std::optional<Index> find(const std::array<double, 3>& position) const
    {
        const auto it = points_.find(quantize(position));
        return it == points_.end() ? std::nullopt : std::optional<Index>(it->second);
    }

private:
    struct QuantizedCoord {
        std::int64_t x;
        std::int64_t y;
        std::int64_t z;

        bool operator==(const QuantizedCoord& other) const
        {
            return x == other.x && y == other.y && z == other.z;
        }
    };

    struct CoordHash {
        std::size_t operator()(const QuantizedCoord& coord) const
        {
            std::size_t hash = 0;
            hash ^= std::hash<std::int64_t>()(coord.x)
                + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<std::int64_t>()(coord.y)
                + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<std::int64_t>()(coord.z)
                + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            return hash;
        }
    };

    static QuantizedCoord quantize(const std::array<double, 3>& position)
    {
        constexpr double tolerance = 1e-7;
        return {
            static_cast<std::int64_t>(std::round(position[0] / tolerance)),
            static_cast<std::int64_t>(std::round(position[1] / tolerance)),
            static_cast<std::int64_t>(std::round(position[2] / tolerance))
        };
    }

    std::unordered_map<QuantizedCoord, Index, CoordHash> points_;
};

/**
 * @brief 将本次 Gmsh 临时状态转换并提交为通用 Geometry↔Mesh 映射
 * @return 转换是否完整成功；失败时保留原映射
 */
bool storeGeometryMeshMap(
    const GmshIncrementalMeshState& state,
    ComponentOperator& component_op)
{
    const MeshData* mesh = component_op.mesh();
    if (!mesh)
        return false;

    GeometryMeshMap candidate;
    const MeshPointLookup point_lookup(*mesh);
    for (const auto& [edge_id, edge_data] : state.meshedEdgesCache) {
        if (edge_data.coords.size() % 3 != 0) {
            spdlog::error("GmshMesh: geometry edge {} has invalid coordinate cache", edge_id);
            return false;
        }

        auto& point_ids = candidate.geometry_edge_to_mesh_point_ids[edge_id];
        point_ids.reserve(edge_data.coords.size() / 3);
        for (std::size_t i = 0; i < edge_data.coords.size(); i += 3) {
            const std::array<double, 3> position {
                edge_data.coords[i], edge_data.coords[i + 1], edge_data.coords[i + 2]
            };
            const auto point_id = point_lookup.find(position);
            if (!point_id) {
                spdlog::error(
                    "GmshMesh: geometry edge {} node is missing from MeshData", edge_id);
                return false;
            }
            point_ids.push_back(*point_id);
        }
    }

    for (const auto& [face_id, result] : state.meshedFacesCache) {
        GeometryFaceMeshTopology topology;
        topology.face_vertices = result.global_face_vertices;
        topology.face_vertices_offset.reserve(result.face_vertices_offset.size());
        for (std::size_t offset : result.face_vertices_offset)
            topology.face_vertices_offset.push_back(static_cast<Index>(offset));
        candidate.geometry_face_to_mesh_topology.emplace(face_id, std::move(topology));
    }

    component_op.editableGeometryMeshMap() = std::move(candidate);
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
    int operationMode = 1;
    IncrementalMeshTools::GmshMeshParameters parameters;

    if (args.size() <= 4) {
        // 兼容旧参数顺序：选择面、网格尺寸、操作模式、网格类型。
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
        parameters.quadMinQuality = readOptionalDouble(args, 9).value_or(0.0);
        parameters.structuredEdgeDivisions = readOptionalInt(args, 10).value_or(0);
    }
    bool writeModel = readBoolArg(args, 11, true);

    if (!validateMeshingParameters(parameters))
        return {};

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

    // 获取或创建当前 component 的 MeshData，Gmsh 生成结果直接写回该 component。
    // 创建经 replaceMesh（gid 纪律内建 + 标脏）；后续逐面循环经 ComponentOperator 可写入口写入
    if (!comp.mesh) {
        spdlog::info("GmshMesh: mesh_data is null, create a new one");
        auto new_mesh = std::make_unique<MeshData>();
        new_mesh->init();
        context.cur_component.replaceMesh(std::move(new_mesh));
    }

    // Gmsh 私有状态只在本次调用中存在；持久身份由可快照的通用映射提供。
    auto state_result = buildGmshState(
        context.cur_component.geometryMeshMap(),
        *context.cur_component.mesh(),
        *geometry,
        modelLayer.geomRegistry());
    if (!state_result)
        return {};
    GmshIncrementalMeshState state = std::move(*state_result);

    if (parameters.targetMeshSize <= 0.0)
        parameters.targetMeshSize = IncrementalMeshTools::estimateMeshSize(*geometry);

    if (operationMode < 1 || operationMode > 3) {
        spdlog::warn("GmshMesh: unknown operation mode {}, skip", operationMode);
        return {};
    }

    std::size_t successCount = 0;
    std::size_t failedCount = 0;
    bool state_changed = false;
    for (GeomFaceId faceId : (*selection)->ids) {
        if (operationMode == 1) {
            if (state.meshedFacesCache.find(faceId) != state.meshedFacesCache.end()) {
                spdlog::info("GmshMesh: face {} already meshed, skip mesh mode", faceId);
                continue;
            }

            auto result = IncrementalMeshTools::meshSingleFace(
                *geometry, state, context.cur_component,
                faceId, parameters.targetMeshSize, parameters);
            if (!result.success) {
                spdlog::warn("GmshMesh: face {} meshing failed", faceId);
                ++failedCount;
                continue;
            }
            state_changed = true;
        } else if (operationMode == 2) {
            if (!IncrementalMeshTools::deleteFaceMesh(
                    *geometry, state, context.cur_component, faceId)) {
                spdlog::warn("GmshMesh: delete face {} failed", faceId);
                ++failedCount;
                continue;
            }
            state_changed = true;
        } else {
            const bool had_face =
                state.meshedFacesCache.find(faceId) != state.meshedFacesCache.end();
            auto result = IncrementalMeshTools::remeshSingleFace(
                *geometry, state, context.cur_component,
                faceId, parameters.targetMeshSize, parameters);
            if (!result.success) {
                spdlog::warn("GmshMesh: face {} remeshing failed", faceId);
                state_changed = state_changed
                    || (had_face
                        && state.meshedFacesCache.find(faceId)
                            == state.meshedFacesCache.end());
                ++failedCount;
                continue;
            }
            state_changed = true;
        }
        ++successCount;
    }

    if (state_changed && !storeGeometryMeshMap(state, context.cur_component)) {
        spdlog::error("GmshMesh: failed to update geometry-mesh mapping");
        return {};
    }

    spdlog::info("GmshMesh: {} selected faces processed: {} succeeded, {} failed",
        (*selection)->ids.size(), successCount, failedCount);

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
        ArgType { ArgTypeEnum::Combo, "操作模式", "划分,删除,重划分" },
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
