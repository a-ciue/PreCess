#pragma once

#include <array>
#include <string>
#include <vector>
#include <filesystem>
#include "GeometryData.h"

struct MeshData;
class ModelLayer;

namespace IncrementalMeshTools {

bool initMeshing(const std::string& stepFile, GeometryData& geometry);

SingleFaceMeshResult meshSingleFace(
    MeshData& mesh_data,
    GeometryData& geometry,
    ModelLayer& model_layer,
    std::size_t faceIndex,
    double meshSize);

SingleFaceMeshResult remeshSingleFace(
    MeshData& mesh_data, 
    GeometryData& geometry,
    ModelLayer& model_layer,
    std::size_t faceIndex,
    double meshSize);

bool deleteFaceMesh(MeshData& mesh_data, GeometryData& geometry, ModelLayer& model_layer, std::size_t faceIndex);

double estimateMeshSize(const GeometryData& geometry);

std::size_t faceCount(const GeometryData& geometry);

std::size_t meshedEdgeCount(const GeometryData& geometry);

bool writeSingleFaceObj(const SingleFaceMeshResult& res, const std::filesystem::path& filepath);

bool writeMeshObj(const MeshData& res, const GeometryData& geometry, const std::filesystem::path& filepath);

} // namespace IncrementalMeshTools
