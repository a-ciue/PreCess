#pragma once

#include <array>
#include <string>
#include <vector>
#include <filesystem>
#include "GmshIncrementalMeshState.h"
#include "GeometryData.h"

struct MeshData;
class ModelLayer;
class GeometryRegistry;

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
    int recombineAlgorithm { -1 };
    double recombineAngle { 45.0 };
    double quadMinQuality {};
    int recombineOptimizeTopology {};
    int structuredEdgeDivisions {};
};

bool initMeshing(
    const std::string& stepFile,
    GeometryData& geometry,
    GmshIncrementalMeshState& state,
    GeometryRegistry& registry);

SingleFaceMeshResult meshSingleFace(
    MeshData& mesh_data,
    GeometryData& geometry,
    GmshIncrementalMeshState& state,
    ModelLayer& model_layer,
    std::size_t faceIndex,
    double meshSize,
    const GmshMeshParameters& parameters);


SingleFaceMeshResult remeshSingleFace(
    MeshData& mesh_data,
    GeometryData& geometry,
    GmshIncrementalMeshState& state,
    ModelLayer& model_layer,
    std::size_t faceIndex,
    double meshSize,
    const GmshMeshParameters& parameters);

bool deleteFaceMesh(
    MeshData& mesh_data,
    GeometryData& geometry,
    GmshIncrementalMeshState& state,
    ModelLayer& model_layer,
    std::size_t faceIndex);

double estimateMeshSize(const GeometryData& geometry);

std::size_t faceCount(const GeometryData& geometry);

std::size_t meshedEdgeCount(const GmshIncrementalMeshState& state);

bool writeSingleFaceObj(const SingleFaceMeshResult& res, const std::filesystem::path& filepath);

bool writeMeshObj(const MeshData& res, const GmshIncrementalMeshState& state, const std::filesystem::path& filepath);

} // namespace IncrementalMeshTools
