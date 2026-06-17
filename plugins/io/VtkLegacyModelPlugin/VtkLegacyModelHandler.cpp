/**
 * @file VtkLegacyModelHandler.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "VtkLegacyModelHandler.h"
#include "ArgType.h"
#include "MeshData.h"
#include "ModelData.h"
#include "UGridModel.h"
#include "ModelLayer.h"

#include <spdlog/spdlog.h>
#include <vtkAppendFilter.h>
#include <vtkCell.h>
#include <vtkCellData.h>
#include <vtkDataSetReader.h>
#include <vtkPointData.h>
#include <vtkUnstructuredGrid.h>
#include <vtkUnstructuredGridWriter.h>
#include <TempFile.h>

namespace systems::io {
static bool is_ascii_path(const std::filesystem::path& p)
{
    auto u8 = p.u8string();
    for (unsigned char ch : u8) {
        if (ch >= 0x80)
            return false;
    }
    return true;
}

static bool ensure_parent_dir(const std::filesystem::path& p)
{
    auto parent = p.parent_path();
    if (parent.empty())
        return true;

    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    if (ec) {
        spdlog::error("VtkLegacyModelHandler: failed to create parent dir '{}': {}",
            parent.string(), ec.message());
        return false;
    }
    return true;
}

static void mesh_from_ugrid(vtkUnstructuredGrid& ugrid, MeshData& out)
{
    out.init();

    // points
    vtkPoints* pts = ugrid.GetPoints();
    if (pts) {
        const vtkIdType n = pts->GetNumberOfPoints();
        out.vertex_positions_.reserve((size_t)n);
        for (vtkIdType i = 0; i < n; ++i) {
            double p[3] {};
            pts->GetPoint(i, p);
            out.vertex_positions_.push_back({ p[0], p[1], p[2] });
        }
    }

    // edges/faces/solids from cells
    // 保证 offsets 起始 0
    out.face_vertices_offset_.clear();
    out.face_vertices_offset_.push_back(0);

    out.solid_vertices_offset_.clear();
    out.solid_vertices_offset_.push_back(0);

    const vtkIdType nCells = ugrid.GetNumberOfCells();
    for (vtkIdType ci = 0; ci < nCells; ++ci) {
        vtkCell* cell = ugrid.GetCell(ci);
        if (!cell)
            continue;

        const int dim = cell->GetCellDimension();
        vtkIdList* ids = cell->GetPointIds();
        if (!ids)
            continue;

        const vtkIdType npts = ids->GetNumberOfIds();
        if (npts <= 0)
            continue;

        if (dim == 1) {
            // line -> edge (仅支持 2 点线段)
            if (npts == 2) {
                out.edge_vertices_.push_back((Index)ids->GetId(0));
                out.edge_vertices_.push_back((Index)ids->GetId(1));
            }
        } else if (dim == 2) {
            // polygon/triangle/quad -> face
            for (vtkIdType k = 0; k < npts; ++k) {
                out.face_vertices_.push_back((Index)ids->GetId(k));
            }
            out.face_vertices_offset_.push_back((Index)out.face_vertices_.size());
        } else if (dim == 3) {
            // 体单元
            out.solid_types_.push_back((unsigned char)cell->GetCellType());
            for (vtkIdType k = 0; k < npts; ++k) {
                out.solid_vertices_.push_back((Index)ids->GetId(k));
            }
            out.solid_vertices_offset_.push_back((Index)out.solid_vertices_.size());

            // polyhedron 才需要 solid_faces_*，一般 hex/tet/wedge/pyramid 不需要
            // 这里保持最小一致性：offset 至少有 {0}
            if (out.solid_faces_offset_.empty())
                out.solid_faces_offset_.push_back(0);
            out.solid_faces_offset_.push_back(0);
        }
    }
}

static void add_cells_from_mesh(vtkUnstructuredGrid& ugrid,
    const std::vector<std::array<double, 3>>& points,
    const MeshData& mesh,
    const std::unordered_map<Index, Index>& global_to_local,
    vtkIdType file_point_offset)
{
    // points already added outside

    auto toPid = [&](Index global_pid) -> vtkIdType {
        auto it = global_to_local.find(global_pid);
        if (it == global_to_local.end())
            return -1;
        return file_point_offset + (vtkIdType)it->second;
    };

    // 1) edges -> VTK_LINE
    if (mesh.edge_vertices_.size() % 2 == 0) {
        for (size_t i = 0; i < mesh.edge_vertices_.size(); i += 2) {
            vtkIdType ids[2] = {
                toPid(mesh.edge_vertices_[i]),
                toPid(mesh.edge_vertices_[i + 1])
            };
            ugrid.InsertNextCell(VTK_LINE, 2, ids);
        }
    }

    // 2) faces -> TRIANGLE/QUAD/POLYGON
    if (mesh.face_vertices_offset_.size() >= 2) {
        const Index nFaces = (Index)mesh.face_vertices_offset_.size() - 1;
        for (Index f = 0; f < nFaces; ++f) {
            const Index a = mesh.face_vertices_offset_[(size_t)f];
            const Index b = mesh.face_vertices_offset_[(size_t)f + 1];
            const Index n = b - a;
            if (n < 3)
                continue;
            if (a < 0 || b < a || b > (Index)mesh.face_vertices_.size())
                continue;

            std::vector<vtkIdType> ids;
            ids.reserve((size_t)n);
            for (Index k = a; k < b; ++k)
                ids.push_back(toPid(mesh.face_vertices_[(size_t)k]));

            const int cellType = (n == 3) ? VTK_TRIANGLE : (n == 4) ? VTK_QUAD
                                                                    : VTK_POLYGON;
            ugrid.InsertNextCell(cellType, (vtkIdType)n, ids.data());
        }
    }

    // 3) solids -> use mesh.solid_types_ as vtk cell type
    if (mesh.solid_vertices_offset_.size() >= 2 && mesh.solid_types_.size() + 1 == mesh.solid_vertices_offset_.size()) {
        const Index nSolids = (Index)mesh.solid_types_.size();
        for (Index si = 0; si < nSolids; ++si) {
            const Index a = mesh.solid_vertices_offset_[(size_t)si];
            const Index b = mesh.solid_vertices_offset_[(size_t)si + 1];
            const Index n = b - a;
            if (n <= 0)
                continue;
            if (a < 0 || b < a || b > (Index)mesh.solid_vertices_.size())
                continue;

            std::vector<vtkIdType> ids;
            ids.reserve((size_t)n);
            for (Index k = a; k < b; ++k)
                ids.push_back(toPid(mesh.solid_vertices_[(size_t)k]));

            ugrid.InsertNextCell((int)mesh.solid_types_[(size_t)si], (vtkIdType)n, ids.data());
        }
    }
}

std::optional<ModelPayload> VtkLegacyModelHandler::read_model(const fs::path& path, const std::vector<std::any>& args)
{
    vtkNew<vtkDataSetReader> reader;
    auto path_string = path.u8string();
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
        return std::nullopt;
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
        return std::nullopt;
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

    auto c = std::make_unique<ComponentData>();
    c->id = -1;
    c->name = "Comp_0";
    c->mesh = std::move(mesh_data);

    ComponentDatas comps;
    comps.push_back(std::move(c));

    return ModelPayload{path.filename().string(), std::move(comps)};
}

void VtkLegacyModelHandler::write_components(const ModelLayer& mgr,
    const std::vector<Index>& component_ids,
    const fs::path& path,
    const std::vector<std::any>&)
{
    if (component_ids.empty()) {
        spdlog::error("VtkLegacyModelHandler: empty component_ids");
        return;
    }

    if (!ensure_parent_dir(path)) {
        return;
    }

    // 1) 构建一个 ugrid：把所有组件的点/单元拼到一起
    vtkNew<vtkUnstructuredGrid> ugrid;
    vtkNew<vtkPoints> points;

    const auto& gp = mgr.globalPoints();
    vtkIdType file_point_offset = 0;

    for (Index cid : component_ids) {
        const ComponentData* comp = mgr.findComponent(cid);
        if (!comp || !comp->mesh)
            continue;

        const MeshData& m = *comp->mesh;
        const Index cnt = m.vertex_count_;
        if (cnt <= 0)
            continue;

        std::unordered_map<Index, Index> global_to_local;
        for (Index i = 0; i < cnt; ++i) {
            global_to_local[m.local_to_global_[i]] = i;
        }

        // add points
        for (Index i = 0; i < cnt; ++i) {
            const Index gid = m.local_to_global_[i];
            const auto& p = gp[(size_t)gid];
            points->InsertNextPoint(p[0], p[1], p[2]);
        }

        // add cells (edge/face/solid)
        add_cells_from_mesh(*ugrid, gp, m, global_to_local, file_point_offset);

        file_point_offset += (vtkIdType)cnt;
    }

    ugrid->SetPoints(points);

    // 2) 处理中文路径：VTK 写中文常失败 -> 写到临时 ASCII 文件，再复制/重命名到目标
    std::filesystem::path real_out = path;
    std::filesystem::path tmp_out;

    if (!is_ascii_path(path)) {
        tmp_out = core::TempFile::instance().path();
        tmp_out.replace_extension(".vtk");
        real_out = tmp_out;
    }

    vtkSmartPointer<vtkUnstructuredGridWriter> writer = vtkSmartPointer<vtkUnstructuredGridWriter>::New();
    writer->SetFileName(real_out.string().c_str());
    writer->SetInputData(ugrid);
    writer->SetFileTypeToASCII();

    const int ok = writer->Write();
    if (!ok) {
        spdlog::error("VtkLegacyModelHandler: writer->Write() failed, out={}", real_out.string());
        return;
    }

    // 3) 如果用临时文件写出，则复制到目标中文路径
    if (!tmp_out.empty()) {
        std::error_code ec;
        std::filesystem::copy_file(tmp_out, path,
            std::filesystem::copy_options::overwrite_existing, ec);
        std::filesystem::remove(tmp_out, ec);
        if (ec) {
            spdlog::error("VtkLegacyModelHandler: copy temp -> target failed, target={}, err={}",
                path.string(), ec.message());
        }
    }
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
