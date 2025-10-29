/**
 * @file MeshMeditModelHandler.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "MeshMeditModelHandler.h"
#include "ArgType.h"
#include "MeshData.h"
#include "ModelData.h"
#include "LibMeshbIO.h"

#include <libmeshb7.h>
#include <spdlog/spdlog.h>
#include <fstream>

namespace systems::io {
std::unique_ptr<ModelData> MeshMeditModelHandler::read_model(const fs::path& path, const std::vector<std::any>& args)
{
    // MeshData
    auto mesh_data = std::make_unique<MeshData>();

    bool success = LibMeshbIO::read(path, *mesh_data);
    if (!success) {
        spdlog::error("Failed to read mesh from file: {}", path.string());
        return nullptr;
    }

    // ModelData
    auto model_data = std::make_unique<ModelData>(std::move(mesh_data));
    model_data->model_name_ = path.filename().string();

    return model_data;
}

void MeshMeditModelHandler::write_model(const ModelData& data, const fs::path& path, const std::vector<std::any>& args)
{
    auto path_str = path.string();
    
    // Get mesh data from ModelData
    const MeshData* mesh_data = data.asMeshData();
    if (!mesh_data) {
        spdlog::error("No mesh data to write");
        return;
    }

    // Default to version 3 (64-bit double precision, supports large files) and 3D
    int version = 3;
    int dimension = 3;
    
    // Check if custom version and dimension are provided in args
    if (args.size() >= 1) {
        try {
            version = std::any_cast<int>(args[0]);
        } catch (const std::bad_any_cast& e) {
            spdlog::warn("Invalid version argument, using default version 3");
        }
    }
    
    if (args.size() >= 2) {
        try {
            dimension = std::any_cast<int>(args[1]);
        } catch (const std::bad_any_cast& e) {
            spdlog::warn("Invalid dimension argument, using default dimension 3");
        }
    }

    // Open file for writing
    int64_t write_idx = GmfOpenMesh(path_str.c_str(), GmfWrite, version, dimension);
    if (!write_idx) {
        spdlog::error("Failed to create mesh file: {}", path_str);
        return;
    }
    
    bool success = LibMeshbIO::write(write_idx, *mesh_data);
    GmfCloseMesh(write_idx);
    
    if (success) {
        spdlog::info("Successfully wrote mesh file: {} (version: {}, dimension: {})", 
                     path_str, version, dimension);
    } else {
        spdlog::error("Failed to write mesh to file: {}", path_str);
    }
}

std::vector<core::ArgType> MeshMeditModelHandler::read_args_type() const
{
    return {};
}

std::vector<core::ArgType> MeshMeditModelHandler::write_args_type() const
{
    return {};
}
}
