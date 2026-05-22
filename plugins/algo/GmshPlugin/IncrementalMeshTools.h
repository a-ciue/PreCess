#pragma once

#include <array>
#include <string>
#include <vector>
#include <filesystem>
#include "SplineData.h"

struct MeshData;

namespace IncrementalMeshTools {

bool initMeshing(const std::string& stepFile, SplineData& spline);

SingleFaceMeshResult meshSingleFace(
    MeshData& mesh_data,
    SplineData& spline,
    std::size_t faceIndex,
    double meshSize);

SingleFaceMeshResult remeshSingleFace(
    MeshData& mesh_data, 
    SplineData& spline,
    std::size_t faceIndex,
    double meshSize);

bool deleteFaceMesh(MeshData& mesh_data, SplineData& spline, std::size_t faceIndex);

double estimateMeshSize(const SplineData& spline);

std::size_t faceCount(const SplineData& spline);

std::size_t meshedEdgeCount(const SplineData& spline);

bool writeSingleFaceObj(const SingleFaceMeshResult& res, const std::filesystem::path& filepath);

bool writeMeshObj(const MeshData& res, const std::filesystem::path& filepath);

} // namespace IncrementalMeshTools