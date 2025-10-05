#include "MakeMeshDataVtk.h"
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

MeshDataVtk MakeMeshDataVtk(
    std::vector<std::array<double, 3>>& vtk_points_,

    std::vector<unsigned char>& vtk_solid_cell_types_,
    std::vector<Index>& vtk_solid_cells_,
    std::vector<Index>& vtk_solid_cells_offset_,
    std::vector<Index>& vtk_solid_faces_,
    std::vector<Index>& vtk_solid_faces_offset_,
    std::vector<Index>& vtk_solid_face_locations_,
    std::vector<Index>& vtk_solid_face_locations_offset_,

    std::vector<Index>& vtk_face_cells_, //> 表示面顶点索引的数组
    std::vector<Index>& vtk_face_cells_offset_,

    std::vector<Index>& vtk_edge_cells_)
{
    // 更复杂的测试模型（删减版本）：
    // Solid: 1个六面体 (Hexahedron) + 1个楔体 (Wedge)
    // Faces: 立方体保留1个四边形面，楔体保留2个三角形面，另外保留2个独立面（1四边形+1三角形）

    // 顶点说明：
    // 0-7   : 立方体
    // 8-11  : 独立四边形
    // 12-13 : 第一条独立线段（用于edges）
    // 14-19 : 楔体 (wedge)
    // 20-22 : 独立三角形
    vtk_points_ = {
        // 立方体 (0-7)
        { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 1.0, 1.0, 0.0 }, { 0.0, 1.0, 0.0 },
        { 0.0, 0.0, 1.0 }, { 1.0, 0.0, 1.0 }, { 1.0, 1.0, 1.0 }, { 0.0, 1.0, 1.0 },
        // 独立四边形 (8-11)
        { 1.5, 0.0, 0.0 }, { 2.5, 0.0, 0.0 }, { 2.5, 1.0, 0.0 }, { 1.5, 1.0, 0.0 },
        // 独立线段1 (12-13)
        { 0.0, 1.5, 0.0 }, { 1.0, 1.5, 0.0 },
        // 楔体 Wedge (14-19)
        { 3.0, 0.0, 0.0 }, { 4.0, 0.0, 0.0 }, { 3.5, 1.0, 0.0 },
        { 3.0, 0.0, 1.0 }, { 4.0, 0.0, 1.0 }, { 3.5, 1.0, 1.0 },
        // 独立三角形 (20-22)
        { 2.0, 1.5, 0.0 }, { 2.5, 1.5, 0.0 }, { 2.25, 2.0, 0.0 }
    };

    // 面（删减）：1个立方体四边形 + 2个楔体三角形 + 2个独立面
    vtk_face_cells_.clear();
    vtk_face_cells_offset_.clear();
    vtk_face_cells_ = {
        // 立方体 1 个四边形
        0, 3, 2, 1, // 面0
        // 楔体 2 个三角形
        14, 15, 16, // 面1
        17, 19, 18, // 面2
        // 独立四边形
        8, 9, 10, 11, // 面3
        // 独立三角形
        20, 21, 22 // 面4
    };
    vtk_face_cells_offset_ = {
        0, // 面0 起始
        4, // 面1 起始
        7, // 面2 起始
        10, // 面3 起始
        14, // 面4 起始
        17 // 结束
    };

    // 边：保持与原示例一致，覆盖立方体、楔体和独立线段
    vtk_edge_cells_ = {
        // 立方体 12 条边 (0-7)
        0, 1, 1, 2, 2, 3, 3, 0,
        4, 5, 5, 6, 6, 7, 7, 4,
        0, 4, 1, 5, 2, 6, 3, 7,
        // 独立线段1
        12, 13,
        // 楔体 9 条边 (底3 + 顶3 + 竖向3)
        14, 15, 15, 16, 16, 14,
        17, 18, 18, 19, 19, 17,
        14, 17, 15, 18, 16, 19,
        // 独立线段2 (三角的一条边)
        20, 21
    };

    // 体：1个六面体 + 1个楔体
    vtk_solid_cell_types_ = { VTK_HEXAHEDRON, VTK_WEDGE };
    vtk_solid_cells_ = {
        // Hex (8点)
        0, 1, 2, 3, 4, 5, 6, 7,
        // Wedge (6点)
        14, 15, 16, 17, 18, 19
    };
    vtk_solid_cells_offset_ = { 0, 8, 14 }; // 2 个 cell

    // polyhedral 相关留空
    vtk_solid_faces_.clear();
    vtk_solid_faces_offset_.clear();
    vtk_solid_face_locations_.clear();
    vtk_solid_face_locations_offset_ = { 0, 0, 0 };

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
        vtk_points_,
        vtk_solid_cell_types_,
        vtk_solid_cells_,
        vtk_solid_cells_offset_,
        vtk_solid_faces_,
        vtk_solid_faces_offset_,
        vtk_solid_face_locations_,
        vtk_solid_face_locations_offset_,
        vtk_face_cells_,
        vtk_face_cells_offset_,
        vtk_edge_cells_,
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
    std::string_view file_path,
    std::vector<std::array<double, 3>>& vtk_points_,
    std::vector<unsigned char>& vtk_solid_cell_types_,
    std::vector<Index>& vtk_solid_cells_,
    std::vector<Index>& vtk_solid_cells_offset_,
    std::vector<Index>& vtk_solid_faces_,
    std::vector<Index>& vtk_solid_faces_offset_,
    std::vector<Index>& vtk_solid_face_locations_,
    std::vector<Index>& vtk_solid_face_locations_offset_,
    std::vector<Index>& vtk_face_cells_,
    std::vector<Index>& vtk_face_cells_offset_,
    std::vector<Index>& vtk_edge_cells_)
{
    // 确保容器干净
    vtk_points_.clear();
    vtk_solid_cell_types_.clear();
    vtk_solid_cells_.clear();
    vtk_solid_cells_offset_.clear();
    vtk_solid_faces_.clear();
    vtk_solid_faces_offset_.clear();
    vtk_solid_face_locations_.clear();
    vtk_solid_face_locations_offset_.clear();
    vtk_face_cells_.clear();
    vtk_face_cells_offset_.clear();
    vtk_edge_cells_.clear();

    vtkSmartPointer<vtkUnstructuredGrid> ug;
    vtkSmartPointer<vtkPolyData> pd;
    if (!readFileToGrid(file_path, ug, pd)) {
        std::cerr << "Cannot read file: " << file_path << ", fallback to built-in demo.\n";
        return MakeMeshDataVtk(
            vtk_points_,
            vtk_solid_cell_types_, vtk_solid_cells_, vtk_solid_cells_offset_,
            vtk_solid_faces_, vtk_solid_faces_offset_, vtk_solid_face_locations_, vtk_solid_face_locations_offset_,
            vtk_face_cells_, vtk_face_cells_offset_, vtk_edge_cells_);
    }

    vtkPoints* pts = ug ? ug->GetPoints() : (pd ? pd->GetPoints() : nullptr);
    if (!pts) {
        std::cerr << "No points in file, fallback demo.\n";
        return MakeMeshDataVtk(
            vtk_points_,
            vtk_solid_cell_types_, vtk_solid_cells_, vtk_solid_cells_offset_,
            vtk_solid_faces_, vtk_solid_faces_offset_, vtk_solid_face_locations_, vtk_solid_face_locations_offset_,
            vtk_face_cells_, vtk_face_cells_offset_, vtk_edge_cells_);
    }
    vtk_points_.resize(static_cast<size_t>(pts->GetNumberOfPoints()));
    double p[3];
    for (vtkIdType i = 0; i < pts->GetNumberOfPoints(); ++i) {
        pts->GetPoint(i, p);
        vtk_points_[static_cast<size_t>(i)] = { p[0], p[1], p[2] };
    }

    auto processCells = [&](vtkDataSet* ds) {
        if (!ds)
            return;
        vtkIdType nCells = ds->GetNumberOfCells();
        vtk_solid_cells_offset_.push_back(0);
        vtk_face_cells_offset_.push_back(0);
        vtk_solid_face_locations_offset_.push_back(0);
        for (vtkIdType cid = 0; cid < nCells; ++cid) {
            vtkCell* cell = ds->GetCell(cid);
            if (!cell)
                continue;
            int dim = cell->GetCellDimension();
            int npts = cell->GetNumberOfPoints();
            if (dim == 3) { // 体
                unsigned char ctype = ds->GetCellType(cid);
                vtk_solid_cell_types_.push_back(ctype);
                for (int i = 0; i < npts; ++i) {
                    vtk_solid_cells_.push_back(static_cast<Index>(cell->GetPointId(i)));
                }
                vtk_solid_cells_offset_.push_back(static_cast<Index>(vtk_solid_cells_.size()));
            } else if (dim == 2) { // 面
                for (int i = 0; i < npts; ++i) {
                    vtk_face_cells_.push_back(static_cast<Index>(cell->GetPointId(i)));
                }
                vtk_face_cells_offset_.push_back(static_cast<Index>(vtk_face_cells_.size()));
            } else if (dim == 1) { // 边或折线
                if (npts >= 2) {
                    for (int i = 0; i < npts - 1; ++i) {
                        vtk_edge_cells_.push_back(static_cast<Index>(cell->GetPointId(i)));
                        vtk_edge_cells_.push_back(static_cast<Index>(cell->GetPointId(i + 1)));
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
    if (vtk_face_cells_offset_.size() > 1) {
        BlockData b;
        b.id = 1; // 所有面放一个block
        int faceCount = static_cast<int>(vtk_face_cells_offset_.size()) - 1;
        for (int f = 0; f < faceCount; ++f)
            b.faces_.push_back(f);
        block_datas->block_datas.push_back(b);
    }

    MeshDataVtk data = {
        vtk_points_,
        vtk_solid_cell_types_,
        vtk_solid_cells_,
        vtk_solid_cells_offset_,
        vtk_solid_faces_,
        vtk_solid_faces_offset_,
        vtk_solid_face_locations_,
        vtk_solid_face_locations_offset_,
        vtk_face_cells_,
        vtk_face_cells_offset_,
        vtk_edge_cells_,
        block_datas
    };
    return data;
}