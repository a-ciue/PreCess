/**
 * @file VtkLegacyModelHandler.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "VtkLegacyModelHandler.h"
#include "ArgType.h"
#include "MeshData.h"
#include "ModelData.h"
#include "UGridModel.h"

#include <fstream>
#include <spdlog/spdlog.h>
#include <vtkAppendFilter.h>
#include <vtkCell.h>
#include <vtkCellData.h>
#include <vtkDataSetReader.h>
#include <vtkPointData.h>
#include <vtkUnstructuredGrid.h>

namespace systems::io {
std::unique_ptr<ModelData> VtkLegacyModelHandler::read_model(const fs::path& path, const std::vector<std::any>& args)
{

    vtkNew<vtkDataSetReader> reader;
    auto path_string = path.string();
    reader->SetFileName(path_string.c_str());
    reader->ReadAllColorScalarsOn();
    reader->ReadAllScalarsOn();
    reader->ReadAllVectorsOn();
    reader->ReadAllFieldsOn();
    reader->ReadAllNormalsOn();
    reader->ReadAllTCoordsOn();
    reader->ReadAllTensorsOn();
    reader->Update();
    vtkDataSet* dataset = reader->GetOutput();
    if (!dataset) {
        spdlog::error("VTK file read failed: {}", path_string);
        return nullptr;
    }

    // 转换为UnstructuredGrid
    vtkSmartPointer<vtkUnstructuredGrid> ugrid;
    if (dataset->GetDataObjectType() == VTK_UNSTRUCTURED_GRID) {
        ugrid = reader->GetUnstructuredGridOutput();
    } else {
        vtkNew<vtkAppendFilter> append_filter;
        append_filter->AddInputData(dataset);
        append_filter->Update();
        ugrid = vtkUnstructuredGrid::SafeDownCast(append_filter->GetOutput());
    }

    if (!ugrid || ugrid->GetNumberOfPoints() == 0) {
        spdlog::error("Failed to convert to vtkUnstructuredGrid: {}", path_string);
        return nullptr;
    }

    // 输出属性信息
    vtkPointData* point_data = ugrid->GetPointData();
    if (point_data) {
        int numArrays = point_data->GetNumberOfArrays();
        spdlog::info("vtkUnstructuredGrid PointData arrays: {}", numArrays);
        for (int i = 0; i < numArrays; ++i) {
            vtkAbstractArray* array = point_data->GetAbstractArray(i);
            spdlog::info("  PointData array[{}]: {}", i, array->GetName());
        }
    }
    vtkCellData* cell_data = ugrid->GetCellData();
    if (cell_data) {
        int numArrays = cell_data->GetNumberOfArrays();
        spdlog::info("vtkUnstructuredGrid CellData arrays: {}", numArrays);
        for (int i = 0; i < numArrays; ++i) {
            vtkAbstractArray* array = cell_data->GetAbstractArray(i);
            spdlog::info("  CellData array[{}]: {}", i, array->GetName());
        }
    }

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

std::vector<core::ArgType> VtkLegacyModelHandler::read_args_type() const
{
    return {};
}

std::vector<core::ArgType> VtkLegacyModelHandler::write_args_type() const
{
    return {};
}
}
