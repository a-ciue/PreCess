#pragma once

#include <array>
#include <string>
#include <vector>
#include <filesystem>
#include "GmshIncrementalMeshState.h"
#include "GeometryData.h"

struct MeshData;
class ModelLayer;

namespace IncrementalMeshTools {

bool initMeshing(const std::string& stepFile, GeometryData& geometry, GmshIncrementalMeshState& state);

SingleFaceMeshResult meshSingleFace(
    MeshData& mesh_data,
    GeometryData& geometry,
    GmshIncrementalMeshState& state,
    ModelLayer& model_layer,
    std::size_t faceIndex,
    double meshSize);

SingleFaceMeshResult remeshSingleFace(
    MeshData& mesh_data, 
    GeometryData& geometry,
    GmshIncrementalMeshState& state,
    ModelLayer& model_layer,
    std::size_t faceIndex,
    double meshSize);

bool deleteFaceMesh(
    MeshData& mesh_data,
    GmshIncrementalMeshState& state,
    ModelLayer& model_layer,
    std::size_t faceIndex);

double estimateMeshSize(const GeometryData& geometry);

std::size_t faceCount(const GmshIncrementalMeshState& state);

std::size_t meshedEdgeCount(const GmshIncrementalMeshState& state);

bool writeSingleFaceObj(const SingleFaceMeshResult& res, const std::filesystem::path& filepath);

bool writeMeshObj(const MeshData& res, const GmshIncrementalMeshState& state, const std::filesystem::path& filepath);

} // namespace IncrementalMeshTools
