#include "UGridModel.h"
#include "MeshData.h"

#include <execution>
#include <spdlog/spdlog.h>
#include <vtkCellData.h>
#include <vtkDoubleArray.h>
#include <vtkPointData.h>
#include <vtkUnstructuredGrid.h>
#include <vector>
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
    // 记录原始 VTK cell id，用于按单元维度拆分 CELL_DATA。
    std::vector<vtkIdType> solid_cell_ids;
    std::vector<vtkIdType> face_cell_ids;

    // cells
    for (vtkIdType cell_id = 0; cell_id < mesh_->GetNumberOfCells(); ++cell_id) {
        vtkCell* cell = mesh_->GetCell(cell_id);
        if (!cell)
            continue;
        vtkIdType dim = cell->GetCellDimension();
        vtkIdType npts = cell->GetNumberOfPoints();
        if (dim == 3) { // 体
            solid_cell_ids.push_back(cell_id);
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
            face_cell_ids.push_back(cell_id);
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

    // 处理cell属性
    vtkCellData* cell_data = mesh_->GetCellData();
    assert(cell_data);
    int cell_arrays = cell_data->GetNumberOfArrays();
    for (int i = 0; i < cell_arrays; i++) {
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
        // 根据记录的cell id，分别提取体属性和面属性
        if (!solid_cell_ids.empty()) {
            std::vector<double> values(solid_cell_ids.size() * num_components);
            std::vector<double> tuple(num_components);
            for (size_t j = 0; j < solid_cell_ids.size(); ++j) {
                array->GetTuple(solid_cell_ids[j], tuple.data());
                std::copy(tuple.begin(), tuple.end(), values.begin() + j * num_components);
            }
            mesh_data.solid_attributes_[array_name] = std::move(values);
        }
        
        if (!face_cell_ids.empty()) {
            std::vector<double> values(face_cell_ids.size() * num_components);
            std::vector<double> tuple(num_components);
            for (size_t j = 0; j < face_cell_ids.size(); ++j) {
                array->GetTuple(face_cell_ids[j], tuple.data());
                std::copy(tuple.begin(), tuple.end(), values.begin() + j * num_components);
            }
            mesh_data.face_attributes_[array_name] = std::move(values);
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
    assert(!mesh_data.solid_vertices_offset_.empty());
    assert(!mesh_data.face_vertices_offset_.empty());
    assert(!mesh_data.solid_faces_offset_.empty());
    assert(!mesh_data.solid_faces_vertices_offset_.empty());

    // point data
    auto points_data = vtkSmartPointer<vtkPoints>::New();
    {
        auto& vtk_points = mesh_data.vertex_positions_;
        auto points_data_array = vtkSmartPointer<vtkDoubleArray>::New();

        points_data_array->SetNumberOfComponents(3);
        points_data_array->SetArray(const_cast<double*>(vtk_points.data()->data()), 3 * vtk_points.size(), 1);
        points_data->SetData(points_data_array);
    }

    // cells
    vtkNew<vtkCellArray> cells;

    vtkNew<vtkAOSDataArrayTemplate<Index>> index_array;
    {
        std::unique_ptr vtk_indices = std::make_unique<Index[]>(mesh_data.solid_vertices_.size() + mesh_data.face_vertices_.size() + mesh_data.edge_vertices_.size());
        std::copy_n(mesh_data.solid_vertices_.data(), mesh_data.solid_vertices_.size(), vtk_indices.get());
        std::copy_n(mesh_data.face_vertices_.data(), mesh_data.face_vertices_.size(), vtk_indices.get() + mesh_data.solid_vertices_.size());
        std::copy_n(mesh_data.edge_vertices_.data(), mesh_data.edge_vertices_.size(), vtk_indices.get() + mesh_data.solid_vertices_.size() + mesh_data.face_vertices_.size());
        index_array->SetArray(vtk_indices.release(), mesh_data.solid_vertices_.size() + mesh_data.face_vertices_.size() + mesh_data.edge_vertices_.size(), 0);
    }

    vtkNew<vtkAOSDataArrayTemplate<Index>> offset_array;
    {
        std::unique_ptr vtk_offsets = std::make_unique<Index[]>(mesh_data.solid_vertices_offset_.size() + mesh_data.face_vertices_offset_.size() - 1 + mesh_data.edge_vertices_.size() / 2);
        vtk_offsets[0] = 0;

        auto& solid_vertices_offset = mesh_data.solid_vertices_offset_;
        std::copy_n(std::execution::par, solid_vertices_offset.data(), solid_vertices_offset.size(), vtk_offsets.get());

        auto& face_vertices_offset = mesh_data.face_vertices_offset_;
        std::transform(std::execution::par, face_vertices_offset.data() + 1, face_vertices_offset.data() + face_vertices_offset.size(), vtk_offsets.get() + solid_vertices_offset.size(),
            [last_offset = solid_vertices_offset.back()](Index cur) { return cur + last_offset; });

        Index* edge_offset_start = vtk_offsets.get() + solid_vertices_offset.size() + face_vertices_offset.size() - 1;
        std::generate_n(std::execution::par, edge_offset_start, mesh_data.edge_vertices_.size() / 2,
            [last_offset = *(edge_offset_start - 1), n = 0]() mutable { n += 2; return last_offset + n; });

        offset_array->SetArray(vtk_offsets.release(), solid_vertices_offset.size() + face_vertices_offset.size() - 1 + mesh_data.edge_vertices_.size() / 2, 0);
    }

    cells->SetData(offset_array, index_array);

    // cell types
    vtkNew<vtkUnsignedCharArray> cell_types;
    {
        size_t face_size = std::max<size_t>(mesh_data.face_vertices_offset_.size(), 1) - 1,
               edge_size = mesh_data.edge_vertices_.size() / 2;

        std::unique_ptr<unsigned char[]> vtk_cell_types = std::make_unique<unsigned char[]>(mesh_data.solid_types_.size() + face_size + edge_size);

        std::copy_n(std::execution::par, mesh_data.solid_types_.data(), mesh_data.solid_types_.size(), vtk_cell_types.get());

        auto& face_vertices_offset = mesh_data.face_vertices_offset_;
        std::transform(std::execution::par, face_vertices_offset.begin(), face_vertices_offset.end() - 1, face_vertices_offset.begin() + 1, vtk_cell_types.get() + mesh_data.solid_types_.size(), [](Index a, Index b) {
            int sides = b - a;
            switch (sides) {
            case 3:
                return VTKCellType::VTK_TRIANGLE;
            case 4:
                return VTKCellType::VTK_QUAD;
            default:
                if (sides > 4) {
                    return VTKCellType::VTK_POLYGON;
                }
                return VTKCellType::VTK_EMPTY_CELL;
            }
        });

        std::fill_n(std::execution::par, vtk_cell_types.get() + mesh_data.solid_types_.size() + face_size, edge_size, VTKCellType::VTK_LINE);

        cell_types->SetArray(vtk_cell_types.release(), mesh_data.solid_types_.size() + face_size + edge_size, 0);
    }

    // faces
    vtkNew<vtkCellArray> faces;
    vtkNew<vtkAOSDataArrayTemplate<Index>> faces_idx;
    auto& vtk_faces = mesh_data.solid_faces_vertices_;
    faces_idx->SetArray(const_cast<Index*>(vtk_faces.data()), vtk_faces.size(), 1);

    vtkNew<vtkAOSDataArrayTemplate<Index>> faces_offset;
    auto& vtk_faces_offset = mesh_data.solid_faces_vertices_offset_;
    faces_offset->SetArray(const_cast<Index*>(vtk_faces_offset.data()), vtk_faces_offset.size(), 1);

    faces->SetData(faces_offset, faces_idx);

    // face locations
    vtkNew<vtkCellArray> face_locations;
    vtkNew<vtkAOSDataArrayTemplate<Index>> face_loc_idx;
    auto& vtk_face_locations = mesh_data.solid_faces_;
    face_loc_idx->SetArray(const_cast<Index*>(vtk_face_locations.data()), vtk_face_locations.size(), 1);
    vtkNew<vtkAOSDataArrayTemplate<Index>> face_loc_offset;
    auto& vtk_face_locations_offset = mesh_data.solid_faces_offset_;
    face_loc_offset->SetArray(const_cast<Index*>(vtk_face_locations_offset.data()), vtk_face_locations_offset.size(), 1);
    face_locations->SetData(face_loc_offset, face_loc_idx);

    // solid ugrid
    this->mesh_->SetPoints(points_data);
    this->mesh_->SetPolyhedralCells(cell_types, cells, face_locations, faces);
}

UGridModel::UGridModel(vtkUnstructuredGrid& mesh)
    : mesh_(&mesh)
{
}

UGridModel::~UGridModel() = default;

std::string UGridModel::completeAttributeName(const std::string& name, int numComponents)
{
    if (numComponents > 0) {
        std::string suffix = "_" + std::to_string(numComponents);
        if (name.size() < suffix.size() || name.substr(name.size() - suffix.size()) != suffix) {
            return name + suffix;
        }
    }
    return name;
}