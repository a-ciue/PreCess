#pragma once

#include <array>
#include <vector>
#include "GmshIncrementalMeshState.h"
#include "GeometryData.h"

class ComponentOperator;

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

// 网格写入统一经 ComponentOperator 可写入口（写必脏：标脏 + gid 纪律内建）
SingleFaceMeshResult meshSingleFace(
    GeometryData& geometry,
    GmshIncrementalMeshState& state,
    ComponentOperator& component_op,
    GeomFaceId faceId,
    double meshSize,
    const GmshMeshParameters& parameters);


SingleFaceMeshResult remeshSingleFace(
    GeometryData& geometry,
    GmshIncrementalMeshState& state,
    ComponentOperator& component_op,
    GeomFaceId faceId,
    double meshSize,
    const GmshMeshParameters& parameters);

bool deleteFaceMesh(
    GeometryData& geometry,
    GmshIncrementalMeshState& state,
    ComponentOperator& component_op,
    GeomFaceId faceId);

double estimateMeshSize(const GeometryData& geometry);

std::size_t faceCount(const GeometryData& geometry);

} // namespace IncrementalMeshTools
