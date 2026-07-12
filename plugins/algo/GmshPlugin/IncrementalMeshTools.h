#pragma once

#include <array>
#include <vector>
#include "GmshIncrementalMeshState.h"
#include "GeometryData.h"

struct MeshData;
class ModelLayer;

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

SingleFaceMeshResult meshSingleFace(
    MeshData& mesh_data,
    GeometryData& geometry,
    GmshIncrementalMeshState& state,
    ModelLayer& model_layer,
    GeomFaceId faceId,
    double meshSize,
    const GmshMeshParameters& parameters);


SingleFaceMeshResult remeshSingleFace(
    MeshData& mesh_data,
    GeometryData& geometry,
    GmshIncrementalMeshState& state,
    ModelLayer& model_layer,
    GeomFaceId faceId,
    double meshSize,
    const GmshMeshParameters& parameters);

bool deleteFaceMesh(
    MeshData& mesh_data,
    GeometryData& geometry,
    GmshIncrementalMeshState& state,
    ModelLayer& model_layer,
    GeomFaceId faceId);

double estimateMeshSize(const GeometryData& geometry);

std::size_t faceCount(const GeometryData& geometry);

} // namespace IncrementalMeshTools
