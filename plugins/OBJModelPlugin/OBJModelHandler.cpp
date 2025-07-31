/**
 * @file OBJModelHandler.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "OBJModelHandler.h"
#include "ModelData.h"
#include "../../CTMeshModel.h"
#include "../../commands/ArgType.h"
#include "MeshData.h"

namespace systems::io {
std::unique_ptr<ModelData> OBJModelHandler::read_model(const fs::path& path, const std::vector<std::any>& args)
{
    // MeshData
    auto mesh_data = std::make_unique<MeshData>();
    CTMeshModel ct_mesh(path);
    ct_mesh.update(*mesh_data);

    // ModelData
    auto model_data = std::make_unique<ModelData>(std::move(mesh_data));
    model_data->model_name_ = path.filename().string();

    return model_data;   // 技巧：RVO
}

void OBJModelHandler::write_model(const ModelData& data, const fs::path& path, const std::vector<std::any>& args)
{
    // TODO: 先实现CTMeshMode::updateFrom(data)方法，可以基于文件IO做
}

std::vector<ArgType> OBJModelHandler::read_args_type() const
{
    return {  };
}

std::vector<ArgType> OBJModelHandler::write_args_type() const
{
    return {};
}
}
