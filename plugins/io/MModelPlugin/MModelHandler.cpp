/**
 * @file MModelHandler.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "MModelHandler.h"
#include "ArgType.h"
#include "MeshData.h"
#include "ModelData.h"
#include "CTMeshModel.h"
#include "ToolMesh.h"

#include <spdlog/spdlog.h>
#include <fstream>

namespace systems::io {
std::unique_ptr<ModelData> MModelHandler::read_model(const fs::path& path, const std::vector<std::any>& args)
{
    // MeshData
    MeshLib::CTMesh mesh;
    mesh.read_m(path.string().c_str());
    auto mesh_data = std::make_unique<MeshData>();
    CTMeshModel model(mesh);
    model.update(*mesh_data);

    // ModelData
    auto model_data = std::make_unique<ModelData>(std::move(mesh_data));
    model_data->model_name_ = path.filename().string();

    return model_data;
}

void MModelHandler::write_model(const ModelData& data, const fs::path& path, const std::vector<std::any>& args)
{
    auto mesh_data = data.asMeshData();
    if (!mesh_data) {
        spdlog::error("MModelHandler only supports writing MeshData.");
    }

    MeshLib::CTMesh mesh;
    CTMeshModel model(mesh);
    model.updateFrom(*mesh_data);

    mesh.write_m(path.string().c_str());
}

std::vector<ArgType> MModelHandler::read_args_type() const
{
    return {};
}

std::vector<ArgType> MModelHandler::write_args_type() const
{
    return {};
}
}
