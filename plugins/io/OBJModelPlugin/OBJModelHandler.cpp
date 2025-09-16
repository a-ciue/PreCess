/**
 * @file OBJModelHandler.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "OBJModelHandler.h"
#include "ArgType.h"
#include "MeshData.h"
#include "ModelData.h"
#include "ObjMeshIO.h"

#include <spdlog/spdlog.h>
#include <fstream>

namespace systems::io {
std::unique_ptr<ModelData> OBJModelHandler::read_model(const fs::path& path, const std::vector<std::any>& args)
{
    // MeshData
    auto mesh_data = ObjMeshIO::loadFromFile(path);

    // ModelData
    auto model_data = std::make_unique<ModelData>(std::move(mesh_data));
    model_data->model_name_ = path.filename().string();

    return model_data; // 技巧：RVO
}

void OBJModelHandler::write_model(const ModelData& data, const fs::path& path, const std::vector<std::any>& args)
{
    auto mesh_data = data.asMeshData();
    if (!mesh_data) {
        spdlog::error("OBJModelHandler only supports writing MeshData.");
    }

    std::ofstream ofs(path);
    ObjMeshIO::saveToFile(*mesh_data, ofs);
}

std::vector<ArgType> OBJModelHandler::read_args_type() const
{
    return {};
}

std::vector<ArgType> OBJModelHandler::write_args_type() const
{
    return {};
}
}
