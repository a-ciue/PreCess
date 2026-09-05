#pragma once

#include <array>
#include <optional>
#include <vector>
#include "GmshIncrementalMeshState.h"
#include "GeometryData.h"

class ComponentOperator;
class GeometryRegistry;
struct GeometryMeshMap;
struct MeshData;

namespace IncrementalMeshTools {

// Gmsh 单面划分参数。空白 UI 参数在 Handler 层会被转换为这里的默认值。
struct GmshMeshParameters {
    double targetMeshSize {};
    double minMeshSize {};
    double maxMeshSize {};
    int meshAlgorithm { 6 };
    int meshTypeIndex {};
    int algorithmSwitchOnFailure {};
    int smoothingSteps {};
    int recombineAlgorithm { 1 };
    double quadMinQuality {};
    int structuredEdgeDivisions {};
};

/**
 * @brief 从通用 Geometry↔Mesh 映射重建本次操作使用的 Gmsh 临时状态
 */
std::optional<GmshIncrementalMeshState> buildStateFromGeometryMeshMap(
    const GeometryMeshMap* mapping,
    const MeshData& mesh,
    const GeometryData& geometry,
    const GeometryRegistry& registry);

/**
 * @brief 将 Gmsh 临时状态转换并提交为通用 Geometry↔Mesh 映射
 * @return 转换是否完整成功；失败时保留原映射
 */
bool storeStateToGeometryMeshMap(
    const GmshIncrementalMeshState& state,
    const GeometryMeshMap& working_mapping,
    ComponentOperator& component_op);

// 网格写入统一经 ComponentOperator 可写入口（写必脏：标脏 + gid 纪律内建）
SingleFaceMeshResult meshSingleFace(
    GeometryData& geometry,
    GmshIncrementalMeshState& state,
    GeometryMeshMap& working_mapping,
    ComponentOperator& component_op,
    GeomFaceId faceId,
    double meshSize,
    const GmshMeshParameters& parameters);


SingleFaceMeshResult remeshSingleFace(
    GeometryData& geometry,
    GmshIncrementalMeshState& state,
    GeometryMeshMap& working_mapping,
    ComponentOperator& component_op,
    GeomFaceId faceId,
    double meshSize,
    const GmshMeshParameters& parameters);

bool deleteFaceMesh(
    GeometryData& geometry,
    GmshIncrementalMeshState& state,
    GeometryMeshMap& working_mapping,
    ComponentOperator& component_op,
    GeomFaceId faceId);

double estimateMeshSize(const GeometryData& geometry);

std::size_t faceCount(const GeometryData& geometry);

} // namespace IncrementalMeshTools
