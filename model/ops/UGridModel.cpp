#include "UGridModel.h"
#include "MeshData.h"

#include <spdlog/spdlog.h>
#include <vtkCellData.h>
#include <vtkDoubleArray.h>
#include <vtkPointData.h>
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

    // 处理顶点属性
    vtkPointData* point_data = mesh_->GetPointData();
    assert(point_data);

    int vertex_arrays = point_data->GetNumberOfArrays();
    for (int i = 0; i < vertex_arrays; i++) {
        vtkAbstractArray* abs_array = point_data->GetAbstractArray(i);
        std::string abs_name = point_data->GetArrayName(i);
        vtkDataArray* array = vtkDataArray::SafeDownCast(abs_array);
        if (!array) {
            spdlog::error("not allowed array:{},Type:{}", abs_name, abs_array->GetArrayType());
            continue; // 跳过非数值型数组
        }
        assert(array);
        std::string array_name = array->GetName();
        int num_components = array->GetNumberOfComponents();
        // 检查是否为多元属性，补全属性名
        array_name = completeAttributeName(array_name, num_components);
        std::vector<double> values;
        values.resize(static_cast<size_t>(pts->GetNumberOfPoints()) * num_components);
        std::vector<double> tuple(num_components);
        for (vtkIdType j = 0; j < pts->GetNumberOfPoints(); ++j) {
            array->GetTuple(j, tuple.data());
            std::copy(tuple.begin(), tuple.end(), values.begin() + j * num_components);
        }
        mesh_data.vertex_attributes_[array_name] = std::move(values);
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

            // 处理面属性
            vtkCellData* cell_data = mesh_->GetCellData();
            assert(cell_data);
            int face_arrays = cell_data->GetNumberOfArrays();
            for (int i = 0; i < face_arrays; i++) {
                vtkAbstractArray* abs_array = cell_data->GetAbstractArray(i);
                std::string abs_name = cell_data->GetArrayName(i);
                vtkDataArray* array = vtkDataArray::SafeDownCast(abs_array);
                if (!array) {
                    spdlog::error("not allowed array:{},Type:{}", abs_name, abs_array->GetArrayType());
                    continue; // 跳过非数值型数组
                }
                assert(array);
                std::string array_name = array->GetName();

                int num_components = array->GetNumberOfComponents();
                array_name = completeAttributeName(array_name, num_components);

                std::vector<double> values;
                values.resize(static_cast<size_t>(mesh_->GetNumberOfCells()) * num_components);
                std::vector<double> tuple(num_components);
                for (vtkIdType j = 0; j < mesh_->GetNumberOfCells(); ++j) {
                    array->GetTuple(j, tuple.data());
                    std::copy(tuple.begin(), tuple.end(), values.begin() + j * num_components);
                }
                mesh_data.face_attributes_[array_name] = std::move(values);
            }
        } else if (dim == 1) { // 边或折线
            if (npts >= 2) {
                for (vtkIdType i = 0; i < npts - 1; ++i) {
                    mesh_data.edge_vertices_.push_back(static_cast<Index>(cell->GetPointId(i)));
                    mesh_data.edge_vertices_.push_back(static_cast<Index>(cell->GetPointId(i + 1)));
                }
            }
        }
    }
    // 输出属性信息进行验证
    // 遍历mesh_data的vertex_attributes
    for (const auto& [name, values] : mesh_data.vertex_attributes_) {
        spdlog::info("Vertex attribute: {}", name);
    }

    // 遍历mesh_data的face_attributes
    for (const auto& [name, values] : mesh_data.face_attributes_) {
        spdlog::info("Face attribute: {}", name);
    }
}

void UGridModel::updateFrom(const MeshData& mesh_data)
{
    // TODO: 从 MeshData 更新 vtkUnstructuredGrid
}

UGridModel::UGridModel(vtkUnstructuredGrid& mesh)
    : mesh_(&mesh)
{
}

UGridModel::~UGridModel() = default;

std::string UGridModel::completeAttributeName(const std::string& name, int numComponents)
{
    if (numComponents > 1) {
        std::string suffix = "_" + std::to_string(numComponents);
        if (name.size() < suffix.size() || name.substr(name.size() - suffix.size()) != suffix) {
            return name + suffix;
        }
    }
    return name;
}