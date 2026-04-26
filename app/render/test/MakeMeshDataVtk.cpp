#include "MakeMeshDataVtk.h"
#include "MakeMeshData.h"

#include <filesystem>
#include <iostream>
#include <vtkCell.h>
#include <vtkCellArray.h>
#include <vtkCellType.h>
#include <vtkDataSet.h>
#include <vtkGenericDataObjectReader.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkXMLUnstructuredGridReader.h>

MeshDataVtk MakeMeshDataVtk(MeshData& data)
{

    if (data.vertex_positions_.empty()) {
        data = MakeMeshData();
    }

    // Block: 根据删减后的面索引重新划分
    // Block1 -> 立方体 1 个面 (0)
    // Block2 -> 楔体    2 个面 (1,2)
    // Block3 -> 独立面  2 个面 (3,4)
    auto block_datas = std::make_shared<BlockDatas>();
    {
        BlockData b1;
        b1.id = 1;
        b1.faces_ = { 0 };
        block_datas->block_datas.push_back(b1);
        BlockData b2;
        b2.id = 2;
        b2.faces_ = { 1, 2 };
        block_datas->block_datas.push_back(b2);
        BlockData b3;
        b3.id = 3;
        b3.faces_ = { 3, 4 };
        block_datas->block_datas.push_back(b3);
    }

    MeshDataVtk test_mesh_data = {
        data.solid_types_,
        data.solid_vertices_,
        data.solid_vertices_offset_,
        data.solid_faces_vertices_,
        data.solid_faces_vertices_offset_,
        data.solid_faces_,
        data.solid_faces_offset_,
        data.face_vertices_,
        data.face_vertices_offset_,
        data.edge_vertices_,
        data.vertex_attributes_,
        data.edge_attributes_,
        data.face_attributes_,
        data.solid_attributes_,
        block_datas
    };

    return test_mesh_data;
}

namespace {
bool readFileToGrid(std::string_view file, vtkSmartPointer<vtkUnstructuredGrid>& ug, vtkSmartPointer<vtkPolyData>& pd)
{
    auto ext = std::filesystem::path(file).extension().string();
    if (ext == ".vtu") {
        auto reader = vtkSmartPointer<vtkXMLUnstructuredGridReader>::New();
        reader->SetFileName(file.data());
        if (!reader->CanReadFile(file.data()))
            return false;
        reader->Update();
        ug = reader->GetOutput();
        return true;
    } else if (ext == ".vtp") {
        auto reader = vtkSmartPointer<vtkXMLPolyDataReader>::New();
        reader->SetFileName(file.data());
        if (!reader->CanReadFile(file.data()))
            return false;
        reader->Update();
        pd = reader->GetOutput();
        return true;
    } else if (ext == ".vtk") {
        // legacy 格式
        auto reader = vtkSmartPointer<vtkGenericDataObjectReader>::New();
        reader->SetFileName(file.data());
        reader->Update();
        if (auto outUG = vtkUnstructuredGrid::SafeDownCast(reader->GetOutput())) {
            ug = outUG;
            return true;
        }
        if (auto outPD = vtkPolyData::SafeDownCast(reader->GetOutput())) {
            pd = outPD;
            return true;
        }
        return false;
    }
    return false;
}
}

MeshDataVtk MakeMeshDataVtkFromFile(
    std::string_view file_path, MeshData& data)
{
    // 确保容器干净
    data.clear();

    vtkSmartPointer<vtkUnstructuredGrid> ug;
    vtkSmartPointer<vtkPolyData> pd;
    if (!readFileToGrid(file_path, ug, pd)) {
        std::cerr << "Cannot read file: " << file_path << ", fallback to built-in demo.\n";
        return MakeMeshDataVtk(data);
    }

    vtkPoints* pts = ug ? ug->GetPoints() : (pd ? pd->GetPoints() : nullptr);
    if (!pts) {
        std::cerr << "No points in file, fallback demo.\n";
        return MakeMeshDataVtk(data);
    }
    data.vertex_positions_.resize(static_cast<size_t>(pts->GetNumberOfPoints()));
    double p[3];
    for (vtkIdType i = 0; i < pts->GetNumberOfPoints(); ++i) {
        pts->GetPoint(i, p);
        data.vertex_positions_[static_cast<size_t>(i)] = { p[0], p[1], p[2] };
    }

    auto processCells = [&](vtkDataSet* ds) {
        if (!ds)
            return;
        vtkIdType nCells = ds->GetNumberOfCells();
        data.solid_vertices_offset_.push_back(0);
        data.face_vertices_offset_.push_back(0);
        data.solid_faces_offset_.push_back(0);
        for (vtkIdType cell_id = 0; cell_id < ds->GetNumberOfCells(); ++cell_id) {
            vtkCell* cell = ds->GetCell(cell_id);
            if (!cell)
                continue;
            vtkIdType dim = cell->GetCellDimension();
            vtkIdType npts = cell->GetNumberOfPoints();
            if (dim == 3) { // 体
                unsigned char ctype = static_cast<unsigned char>(ds->GetCellType(cell_id));
                data.solid_types_.push_back(ctype);
                for (vtkIdType i = 0; i < npts; ++i) {
                    data.solid_vertices_.push_back(static_cast<Index>(cell->GetPointId(i)));
                }
                data.solid_vertices_offset_.push_back(static_cast<Index>(data.solid_vertices_.size()));

                // 对多面体单元，处理其面的信息
                if (cell->GetCellType() == VTK_POLYHEDRON) {
                    vtkIdType nfaces = cell->GetNumberOfFaces();
                    data.solid_faces_offset_.push_back(static_cast<Index>(data.solid_faces_.size()));
                    for (vtkIdType fid = 0; fid < nfaces; ++fid) {
                        vtkCell* face = cell->GetFace(fid);
                        if (!face)
                            continue;
                        vtkIdType fnpts = face->GetNumberOfPoints();
                        data.solid_faces_vertices_offset_.push_back(static_cast<Index>(data.solid_faces_vertices_.size()));
                        for (vtkIdType i = 0; i < fnpts; ++i) {
                            data.solid_faces_vertices_.push_back(static_cast<Index>(face->GetPointId(i)));
                        }
                        data.solid_faces_.push_back(static_cast<Index>(data.solid_faces_vertices_offset_.size() - 1));
                    }
                    data.solid_faces_vertices_offset_.push_back(static_cast<Index>(data.solid_faces_vertices_.size()));
                } else {
                    // 非多面体，补充offset
                    data.solid_faces_offset_.push_back(static_cast<Index>(data.solid_faces_.size()));
                }
            } else if (dim == 2) { // 面
                for (vtkIdType i = 0; i < npts; ++i) {
                    data.face_vertices_.push_back(static_cast<Index>(cell->GetPointId(i)));
                }
                data.face_vertices_offset_.push_back(static_cast<Index>(data.face_vertices_.size()));
            } else if (dim == 1) { // 边或折线
                if (npts >= 2) {
                    for (vtkIdType i = 0; i < npts - 1; ++i) {
                        data.edge_vertices_.push_back(static_cast<Index>(cell->GetPointId(i)));
                        data.edge_vertices_.push_back(static_cast<Index>(cell->GetPointId(i + 1)));
                    }
                }
            }
        }
    };

    if (ug)
        processCells(ug);
    if (pd)
        processCells(pd);

    auto block_datas = std::make_shared<BlockDatas>();
    if (data.face_vertices_offset_.size() > 1) {
        BlockData b;
        b.id = 1; // 所有面放一个block
        int faceCount = static_cast<int>(data.face_vertices_offset_.size()) - 1;
        for (int f = 0; f < faceCount; ++f)
            b.faces_.push_back(f);
        block_datas->block_datas.push_back(b);
    }

    MeshDataVtk res = {
        data.solid_types_,
        data.solid_vertices_,
        data.solid_vertices_offset_,
        data.solid_faces_vertices_,
        data.solid_faces_vertices_offset_,
        data.solid_faces_,
        data.solid_faces_offset_,
        data.face_vertices_,
        data.face_vertices_offset_,
        data.edge_vertices_,
        data.vertex_attributes_,
        data.edge_attributes_,
        data.face_attributes_,
        data.solid_attributes_,
        block_datas
    };
    return res;
}