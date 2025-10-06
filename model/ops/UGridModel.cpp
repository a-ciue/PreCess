#include "UGridModel.h"
#include "MeshData.h"

#include <spdlog/spdlog.h>
#include <vtkUnstructuredGrid.h>

void UGridModel::update(MeshData& mesh_data)
{
    using namespace std;
    mesh_data.init();

    // points
    vtkPoints* pts = mesh_->GetPoints();
    if (!pts) {
        spdlog::error("UGridModel::update: No points in the vtkUnstructuredGrid.");
        return;
    }
    mesh_data.vertex_positions_.reserve(static_cast<size_t>(pts->GetNumberOfPoints()));
    std::array<double, 3> p;
    for (vtkIdType i = 0; i < pts->GetNumberOfPoints(); ++i) {
        pts->GetPoint(i, p.data());
        mesh_data.vertex_positions_.emplace_back(p);
    }

    // cells
    for (vtkIdType cell_id = 0; cell_id < mesh_->GetNumberOfCells(); ++cell_id) {
        vtkCell* cell = mesh_->GetCell(cell_id);
        if (!cell)
            continue;
        vtkIdType dim = cell->GetCellDimension();
        vtkIdType npts = cell->GetNumberOfPoints();
        if (dim == 3) { // 体
            unsigned char ctype = static_cast<unsigned char>(mesh_->GetCellType(cell_id));
            mesh_data.solid_types_.push_back(ctype);
            for (vtkIdType i = 0; i < npts; ++i) {
                mesh_data.solid_vertices_.push_back(static_cast<Index>(cell->GetPointId(i)));
            }
            mesh_data.solid_vertices_offset_.push_back(static_cast<Index>(mesh_data.solid_vertices_.size()));

            // 对多面体单元，处理其面的信息
            if (cell->GetCellType() == VTK_POLYHEDRON) {
                vtkIdType nfaces = cell->GetNumberOfFaces();
                mesh_data.solid_faces_offset_.push_back(static_cast<Index>(mesh_data.solid_faces_.size()));
                for (vtkIdType fid = 0; fid < nfaces; ++fid) {
                    vtkCell* face = cell->GetFace(fid);
                    if (!face)
                        continue;
                    vtkIdType fnpts = face->GetNumberOfPoints();
                    mesh_data.solid_faces_vertices_offset_.push_back(static_cast<Index>(mesh_data.solid_faces_vertices_.size()));
                    for (vtkIdType i = 0; i < fnpts; ++i) {
                        mesh_data.solid_faces_vertices_.push_back(static_cast<Index>(face->GetPointId(i)));
                    }
                    mesh_data.solid_faces_.push_back(static_cast<Index>(mesh_data.face_vertices_offset_.size() - 1));
                }
                mesh_data.solid_faces_vertices_offset_.push_back(static_cast<Index>(mesh_data.solid_faces_vertices_.size()));
            } else {
                // 非多面体，补充offset
                mesh_data.solid_faces_offset_.push_back(static_cast<Index>(mesh_data.solid_faces_.size()));
            }
        } else if (dim == 2) { // 面
            for (vtkIdType i = 0; i < npts; ++i) {
                mesh_data.face_vertices_.push_back(static_cast<Index>(cell->GetPointId(i)));
            }
            mesh_data.face_vertices_offset_.push_back(static_cast<Index>(mesh_data.face_vertices_.size()));
        } else if (dim == 1) { // 边或折线
            if (npts >= 2) {
                for (vtkIdType i = 0; i < npts - 1; ++i) {
                    mesh_data.edge_vertices_.push_back(static_cast<Index>(cell->GetPointId(i)));
                    mesh_data.edge_vertices_.push_back(static_cast<Index>(cell->GetPointId(i + 1)));
                }
            }
        }
    }
}

void UGridModel::updateFrom(const MeshData& mesh_data)
{
    // TODO: 从 MeshData 更新 vtkUnstructuredGrid
}


UGridModel::UGridModel(vtkUnstructuredGrid& mesh)
    :mesh_(&mesh)
{
}

UGridModel::~UGridModel() = default;