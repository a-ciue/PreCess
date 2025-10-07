/**
 * @file VtkLegacyModelHandler.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "VtkLegacyModelHandler.h"
#include "ArgType.h"
#include "MeshData.h"
#include "ModelData.h"
#include "UGridModel.h"

#include <vtkDataSetReader.h>
#include <spdlog/spdlog.h>
#include <fstream>

namespace systems::io {
std::unique_ptr<ModelData> VtkLegacyModelHandler::read_model(const fs::path& path, const std::vector<std::any>& args)
{
    // MeshData
    vtkNew<vtkDataSetReader> reader;
    auto path_string = path.string();
    reader->SetFileName(path_string.c_str());
    reader->Update();

    vtkUnstructuredGrid* ugrid = reader->GetUnstructuredGridOutput();
    UGridModel ugrid_model(*ugrid);
    auto mesh_data = std::make_unique<MeshData>();
    ugrid_model.update(*mesh_data);

    // ModelData
    auto model_data = std::make_unique<ModelData>(std::move(mesh_data));
    model_data->model_name_ = path.filename().string();

    return model_data;
}

void VtkLegacyModelHandler::write_model(const ModelData& data, const fs::path& path, const std::vector<std::any>& args)
{
    // TODO: implement
}

std::vector<ArgType> VtkLegacyModelHandler::read_args_type() const
{
    return {};
}

std::vector<ArgType> VtkLegacyModelHandler::write_args_type() const
{
    return {};
}
}
