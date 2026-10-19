#include "MeshPipelineData.h"
#include "FaceHideFilter.h"
#include "MeshData.h"

#include <algorithm>
#include <atomic>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <vtkAOSDataArrayTemplate.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkDoubleArray.h>
#include <vtkExtractEdges.h>
#include <vtkExtractGeometry.h>
#include <vtkExtractPolyDataGeometry.h>
#include <vtkGeometryFilter.h>
#include <vtkIdTypeArray.h>
#include <vtkPlane.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkSMPTools.h>
#include <vtkUnsignedCharArray.h>
#include <vtkUnstructuredGrid.h>

namespace {
std::atomic_uint64_t next_mesh_generation { 0 };

/** @brief 只复制数据集包装；坐标、属性和连通性数组在发布后只读共享。 */
vtkSmartPointer<vtkPolyData> publishPolyData(vtkPolyData* input)
{
    auto output = vtkSmartPointer<vtkPolyData>::New();
    output->ShallowCopy(input);
    if (input->GetPoints()) {
        vtkNew<vtkPoints> points;
        points->SetData(input->GetPoints()->GetData());
        output->SetPoints(points);
    }
    auto wrap_cells = [](vtkCellArray* cells) {
        auto wrapped = vtkSmartPointer<vtkCellArray>::New();
        if (cells)
            wrapped->ShallowCopy(cells);
        return wrapped;
    };
    // 独立的单元包装拥有新的 MTime，避免隐藏恢复后的拾取和线框缓存复用旧状态。
    output->SetVerts(wrap_cells(input->GetVerts()));
    output->SetLines(wrap_cells(input->GetLines()));
    output->SetPolys(wrap_cells(input->GetPolys()));
    output->SetStrips(wrap_cells(input->GetStrips()));
    return output;
}

// 将组件局部点属性写入 PointData，供面/体 mapper 按点属性渲染。
void addPointAttributes(
    vtkPointData& point_data,
    const std::map<std::string, std::vector<double>>& attributes,
    vtkIdType point_count)
{
    if (point_count <= 0)
        return;

    const size_t num_points = static_cast<size_t>(point_count);
    for (const auto& [attr_name, attr_values] : attributes) {
        if (attr_values.empty())
            continue;

        // 属性数组按 [point0 分量..., point1 分量...] 存放，总长度必须是点数的整数倍。
        if (attr_values.size() % num_points != 0) {
            spdlog::error("Attribute {} size mismatch: {} values for {} points",
                attr_name, attr_values.size(), num_points);
            continue;
        }

        // PointData 是当前 VTK 数据对象自己的局部点数组，tuple 与局部点 id 一一对应。
        const size_t ncomp = attr_values.size() / num_points;
        auto array = vtkSmartPointer<vtkDoubleArray>::New();
        array->SetNumberOfComponents(static_cast<int>(ncomp));
        array->SetName(attr_name.c_str());
        array->SetNumberOfTuples(point_count);
        for (size_t i = 0; i < num_points; ++i)
            array->SetTuple(static_cast<vtkIdType>(i), &attr_values[i * ncomp]);
        point_data.AddArray(array);
    }
}

// 将边、面或体属性写入对应 VTK 数据对象的 CellData。
void addCellAttributes(
    vtkCellData& cell_data,
    const std::map<std::string, std::vector<double>>& attributes,
    vtkIdType cell_count,
    const char* cell_kind)
{
    if (cell_count <= 0)
        return;

    const size_t num_cells = static_cast<size_t>(cell_count);
    for (const auto& [attr_name, attr_values] : attributes) {
        // CellData 是当前 VTK 数据对象自己的局部 cell 数组，不需要全局偏移。
        if (attr_values.size() < num_cells) {
            spdlog::error("Attribute {} has insufficient values for {} cells", attr_name, cell_kind);
            continue;
        }
        if (attr_values.size() % num_cells != 0) {
            spdlog::error("Attribute {} size mismatch: {} values for {} {} cells",
                attr_name, attr_values.size(), num_cells, cell_kind);
            continue;
        }

        const size_t ncomp = attr_values.size() / num_cells;
        auto array = vtkSmartPointer<vtkDoubleArray>::New();
        array->SetNumberOfComponents(static_cast<int>(ncomp));
        array->SetName(attr_name.c_str());
        array->SetNumberOfTuples(cell_count);
        for (size_t i = 0; i < num_cells; ++i)
            array->SetTuple(static_cast<vtkIdType>(i), &attr_values[i * ncomp]);
        cell_data.AddArray(array);
    }
}

/** @brief 从自持连通性建立体网格，保留多面体的面与面位置索引。 */
void createSolidGrid(const MeshDataVtk& model_data, vtkPoints& points, vtkUnstructuredGrid& solid_data)
{
    // 以下 SetArray 的 save=1 禁止 VTK 释放外部数组；缓冲由快照中的源网格保活。
    // cells
    vtkNew<vtkCellArray> solid_cells;
    vtkNew<vtkAOSDataArrayTemplate<Index>> index_array;
    auto& vtk_indices = model_data.vtk_solid_cells_;
    index_array->SetArray(const_cast<Index*>(vtk_indices.data()), vtk_indices.size(), 1);

    vtkNew<vtkAOSDataArrayTemplate<Index>> offset_array;
    auto& vtk_offsets = model_data.vtk_solid_cells_offset_;
    offset_array->SetArray(const_cast<Index*>(vtk_offsets.data()), vtk_offsets.size(), 1);

    solid_cells->SetData(offset_array, index_array);

    // cell types
    vtkNew<vtkUnsignedCharArray> cell_types;
    auto& types = model_data.vtk_solid_cell_types_;
    cell_types->SetArray(const_cast<unsigned char*>(types.data()), types.size(), 1);

    // faces
    vtkNew<vtkCellArray> faces;
    vtkNew<vtkAOSDataArrayTemplate<Index>> faces_idx;
    auto& vtk_faces = model_data.vtk_solid_faces_;
    faces_idx->SetArray(const_cast<Index*>(vtk_faces.data()), vtk_faces.size(), 1);

    vtkNew<vtkAOSDataArrayTemplate<Index>> faces_offset;
    auto& vtk_faces_offset = model_data.vtk_solid_faces_offset_;
    faces_offset->SetArray(const_cast<Index*>(vtk_faces_offset.data()), vtk_faces_offset.size(), 1);

    faces->SetData(faces_offset, faces_idx);

    // face locations
    vtkNew<vtkCellArray> face_locations;
    vtkNew<vtkAOSDataArrayTemplate<Index>> face_loc_idx;
    auto& vtk_face_locations = model_data.vtk_solid_face_locations_;
    face_loc_idx->SetArray(const_cast<Index*>(vtk_face_locations.data()), vtk_face_locations.size(), 1);
    vtkNew<vtkAOSDataArrayTemplate<Index>> face_loc_offset;
    auto& vtk_face_locations_offset = model_data.vtk_solid_face_locations_offset_;
    face_loc_offset->SetArray(const_cast<Index*>(vtk_face_locations_offset.data()), vtk_face_locations_offset.size(), 1);
    face_locations->SetData(face_loc_offset, face_loc_idx);

    // solid ugrid
    solid_data.SetPoints(&points);
    solid_data.SetPolyhedralCells(cell_types, solid_cells, face_locations, faces);
}

}

MeshPipelineData::MeshPipelineData()
    : owner_thread_(std::this_thread::get_id())
{
}

MeshPipelineData::~MeshPipelineData() = default;

void MeshPipelineData::assertOwnerThread() const
{
    if (owner_thread_ != std::this_thread::get_id())
        throw std::logic_error("MeshPipelineData must be used on its creating thread");
}

void MeshPipelineData::loadModelData(const MeshDataVtk& data)
{
    assertOwnerThread();
    // MeshDataVtk 仅是引用视图，捕获一次自持缓冲；不对 VTK 数据做全量 DeepCopy。
    auto owned = std::make_shared<MeshData>();
    owned->vertex_positions_ = data.vertex_positions_;
    owned->solid_types_ = data.vtk_solid_cell_types_;
    owned->solid_vertices_ = data.vtk_solid_cells_;
    owned->solid_vertices_offset_ = data.vtk_solid_cells_offset_;
    owned->solid_faces_vertices_ = data.vtk_solid_faces_;
    owned->solid_faces_vertices_offset_ = data.vtk_solid_faces_offset_;
    owned->solid_faces_ = data.vtk_solid_face_locations_;
    owned->solid_faces_offset_ = data.vtk_solid_face_locations_offset_;
    owned->face_vertices_ = data.vtk_face_cells_;
    owned->face_vertices_offset_ = data.vtk_face_cells_offset_;
    owned->edge_vertices_ = data.vtk_edge_cells_;
    owned->vertex_attributes_ = data.vertex_attributes_;
    owned->edge_attributes_ = data.edge_attributes_;
    owned->face_attributes_ = data.face_attributes_;
    owned->solid_attributes_ = data.solid_attributes_;
    owned->vertex_count_ = static_cast<Index>(owned->vertex_positions_.size());
    auto blocks = data.model_blocks_ ? std::make_shared<BlockDatas>(*data.model_blocks_) : nullptr;
    source_ = owned;
    model_data_ = std::make_shared<MeshDataVtk>(MeshDataVtk {
        owned->solid_types_, owned->solid_vertices_, owned->solid_vertices_offset_,
        owned->solid_faces_vertices_, owned->solid_faces_vertices_offset_,
        owned->solid_faces_, owned->solid_faces_offset_,
        owned->face_vertices_, owned->face_vertices_offset_, owned->edge_vertices_,
        owned->vertex_positions_, owned->vertex_attributes_, owned->edge_attributes_,
        owned->face_attributes_, owned->solid_attributes_, blocks, data.component_id });
    face_data_ = vtkSmartPointer<vtkPolyData>::New();
    edge_data_ = vtkSmartPointer<vtkPolyData>::New();
    solid_data_ = vtkSmartPointer<vtkUnstructuredGrid>::New();
    vtkNew<vtkPoints> points_;
    vtkNew<vtkIdTypeArray> original_point_ids_;
    generation_ = ++next_mesh_generation;
    hidden_face_ids_.clear();
    dirty_ = true;
    // 本方法所有 SetArray 均使用 save=1：VTK 借用 vector 缓冲，不负责释放。
    // source_ 与已发布快照共同保活这些缓冲，不能把 save 改为 0。
    // 组件私有点集：坐标自持，连通性数组直接以局部点 id 作 VTK 点索引
    const auto& positions = model_data_->vertex_positions_;
    const vtkIdType point_count = static_cast<vtkIdType>(positions.size());
    vtkNew<vtkAOSDataArrayTemplate<double>> point_array;
    point_array->SetNumberOfComponents(3);
    if (point_count > 0)
        point_array->SetArray(const_cast<double*>(positions.front().data()), point_count * 3, 1);
    points_->SetData(point_array);

    // 拾取标签 vtkOriginalPointIds：存局部点 id，高亮提取直接作 VTK 点索引；
    // 与提取过滤器自动生成的同名数组同语义（均为局部点下标），裁剪链上被覆盖也不影响取值
    original_point_ids_->SetName("vtkOriginalPointIds");
    original_point_ids_->SetNumberOfComponents(1);
    original_point_ids_->SetNumberOfTuples(point_count);
    vtkSMPTools::For(0, point_count,
        [&](vtkIdType begin, vtkIdType end) {
            for (vtkIdType id = begin; id < end; ++id) {
                original_point_ids_->SetValue(id, static_cast<Index>(id));
            }
        });
    original_point_ids_->Modified();

    // face data
    vtkPolyData* face_poly = this->face_data_;
    {
        auto poly_data = vtkSmartPointer<vtkCellArray>::New();
        auto index_array = vtkSmartPointer<vtkAOSDataArrayTemplate<Index>>::New();
        auto& vtk_indices = this->model_data_->vtk_face_cells_;
        index_array->SetArray(const_cast<Index*>(vtk_indices.data()), vtk_indices.size(), 1);

        auto offset_array = vtkSmartPointer<vtkAOSDataArrayTemplate<Index>>::New();
        auto& vtk_offsets = this->model_data_->vtk_face_cells_offset_;
        offset_array->SetArray(const_cast<Index*>(vtk_offsets.data()), vtk_offsets.size(), 1);

        poly_data->SetData(offset_array, index_array);

        // face poly data
        face_poly->SetPoints(points_);
        face_poly->SetPolys(poly_data);

        face_poly->GetPointData()->AddArray(original_point_ids_.GetPointer());

        // 面索引数组：不裁剪时索引即模型面 id；随裁剪链透传，裁剪开启时拾取仍能映射回模型面
        vtkNew<vtkIdTypeArray> face_ids;
        face_ids->SetNumberOfComponents(1);
        face_ids->SetName("PrecessFaceIds");
        face_ids->SetNumberOfTuples(face_poly->GetNumberOfCells());
        vtkSMPTools::For(0, face_poly->GetNumberOfCells(),
            [&](vtkIdType begin, vtkIdType end) {
                for (vtkIdType id = begin; id < end; ++id) {
                    face_ids->SetValue(id, id);
                }
            });
        face_poly->GetCellData()->AddArray(face_ids);

        // 处理面属性
        addPointAttributes(*face_poly->GetPointData(), model_data_->vertex_attributes_, point_count);
        addCellAttributes(*face_poly->GetCellData(), model_data_->face_attributes_,
            face_poly->GetNumberOfCells(), "face");
    }

    // edge data
    vtkPolyData* edge_poly = this->edge_data_;
    {
        vtkNew<vtkCellArray> edge_cells;
        vtkNew<vtkAOSDataArrayTemplate<Index>> index_array;
        auto& vtk_indices = this->model_data_->vtk_edge_cells_;
        index_array->SetArray(const_cast<Index*>(vtk_indices.data()), vtk_indices.size(), 1);
        edge_cells->SetData(2, index_array);

        edge_poly->SetPoints(points_);
        edge_poly->SetLines(edge_cells);

        edge_poly->GetPointData()->AddArray(original_point_ids_.GetPointer());
        addCellAttributes(*edge_poly->GetCellData(), model_data_->edge_attributes_,
            edge_poly->GetNumberOfCells(), "edge");
    }

    // solid data
    vtkUnstructuredGrid* solid_ugird = this->solid_data_;
    createSolidGrid(*this->model_data_, *points_, *solid_ugird);
    vtkIdType solid_cells_count = solid_ugird->GetNumberOfCells();
    vtkNew<vtkIdTypeArray> originalCellIds;
    originalCellIds->SetNumberOfComponents(1);
    originalCellIds->SetName("vtkOriginalCellIds");
    originalCellIds->SetNumberOfTuples(solid_cells_count);
    // 使用并行方式设置原始单元ID
    vtkSMPTools::For(0, solid_cells_count,
        [&](vtkIdType begin, vtkIdType end) {
            for (vtkIdType cellId = begin; cellId < end; ++cellId) {
                originalCellIds->SetValue(cellId, cellId);
            }
        });
    solid_ugird->GetCellData()->AddArray(originalCellIds);
    solid_ugird->GetPointData()->AddArray(original_point_ids_.GetPointer());
    // 处理体属性
    addPointAttributes(*solid_ugird->GetPointData(), model_data_->vertex_attributes_, point_count);
    addCellAttributes(*solid_ugird->GetCellData(), model_data_->solid_attributes_, solid_cells_count, "solid");
}

void MeshPipelineData::setVisibility(bool visible)
{
    assertOwnerThread();
    visible_ = visible;
}

void MeshPipelineData::hideFaces(const std::vector<vtkIdType>& face_ids)
{
    assertOwnerThread();
    if (!face_data_)
        return;
    const auto count = face_data_->GetNumberOfCells();
    for (auto id : face_ids) {
        if (id >= 0 && id < count)
            hidden_face_ids_.push_back(id);
    }
    std::sort(hidden_face_ids_.begin(), hidden_face_ids_.end());
    hidden_face_ids_.erase(std::unique(hidden_face_ids_.begin(), hidden_face_ids_.end()), hidden_face_ids_.end());
    dirty_ = true;
}

void MeshPipelineData::showFaces(const std::vector<vtkIdType>& face_ids)
{
    assertOwnerThread();
    if (!face_data_)
        return;
    std::vector<vtkIdType> valid_ids;
    for (auto id : face_ids) {
        if (id >= 0 && id < face_data_->GetNumberOfCells())
            valid_ids.push_back(id);
    }
    if (valid_ids.empty())
        return;
    if (!visible_) {
        isolateFaces(valid_ids);
        return;
    }
    std::sort(valid_ids.begin(), valid_ids.end());
    std::erase_if(hidden_face_ids_, [&valid_ids](auto id) {
        return std::binary_search(valid_ids.begin(), valid_ids.end(), id);
    });
    dirty_ = true;
}

void MeshPipelineData::isolateFaces(const std::vector<vtkIdType>& face_ids)
{
    assertOwnerThread();
    if (!face_data_)
        return;
    std::vector<bool> shown(static_cast<std::size_t>(face_data_->GetNumberOfCells()), false);
    for (auto id : face_ids) {
        if (id >= 0 && id < static_cast<vtkIdType>(shown.size()))
            shown[static_cast<std::size_t>(id)] = true;
    }
    hidden_face_ids_.clear();
    for (vtkIdType id = 0; id < static_cast<vtkIdType>(shown.size()); ++id) {
        if (!shown[static_cast<std::size_t>(id)])
            hidden_face_ids_.push_back(id);
    }
    visible_ = true;
    dirty_ = true;
}

void MeshPipelineData::showAllFaces()
{
    assertOwnerThread();
    if (!hidden_face_ids_.empty()) {
        hidden_face_ids_.clear();
        dirty_ = true;
    }
}

bool MeshPipelineData::reverseVisibility()
{
    assertOwnerThread();
    if (!visible_) {
        showAllFaces();
        visible_ = true;
    } else if (hidden_face_ids_.empty()) {
        visible_ = false;
    } else if (face_data_) {
        std::vector<vtkIdType> reversed;
        for (vtkIdType id = 0; id < face_data_->GetNumberOfCells(); ++id) {
            if (!std::binary_search(hidden_face_ids_.begin(), hidden_face_ids_.end(), id))
                reversed.push_back(id);
        }
        hidden_face_ids_ = std::move(reversed);
        dirty_ = true;
    }
    return visible_;
}

void MeshPipelineData::setClipPlane(vtkPlane* plane)
{
    assertOwnerThread();
    MeshClipPlane parameters;
    parameters.enabled = plane != nullptr;
    if (plane) {
        plane->GetOrigin(parameters.origin.data());
        plane->GetNormal(parameters.normal.data());
    }
    setClipPlane(parameters);
}

void MeshPipelineData::setClipPlane(const MeshClipPlane& plane)
{
    assertOwnerThread();
    clip_ = plane;
    dirty_ = true;
}

void MeshPipelineData::rebuildOutputs()
{
    // 每次重建使用新的 Filter，下一次 Update 不会改写已发布快照引用的输出数组。
    vtkNew<FaceHideFilter> hider;
    vtkNew<vtkIdTypeArray> hidden_ids;
    for (auto id : hidden_face_ids_)
        hidden_ids->InsertNextValue(id);
    hider->SetInputData(face_data_);
    hider->SetHiddenCellIds(hidden_ids);
    hider->Update();

    vtkNew<vtkPlane> plane;
    plane->SetOrigin(clip_.origin.data());
    plane->SetNormal(clip_.normal.data());
    vtkNew<vtkExtractPolyDataGeometry> face_clipper;
    vtkNew<vtkExtractPolyDataGeometry> edge_clipper;
    vtkNew<vtkExtractGeometry> solid_clipper;
    vtkPolyData* faces = hider->GetOutput();
    vtkPolyData* edges = edge_data_;
    vtkUnstructuredGrid* solids = solid_data_;
    if (clip_.enabled) {
        face_clipper->SetInputData(faces);
        face_clipper->SetImplicitFunction(plane);
        face_clipper->Update();
        faces = face_clipper->GetOutput();
        edge_clipper->SetInputData(edges);
        edge_clipper->SetImplicitFunction(plane);
        edge_clipper->Update();
        edges = edge_clipper->GetOutput();
        solid_clipper->SetInputData(solids);
        solid_clipper->SetImplicitFunction(plane);
        solid_clipper->Update();
        solids = solid_clipper->GetOutput();
    }
    vtkNew<vtkGeometryFilter> surface;
    surface->SetInputData(solids);
    surface->Update();
    vtkNew<vtkExtractEdges> internal_edges;
    // 保持现有内部线框样式：从完整原始体提边，不在此次迁移中改变裁剪行为。
    internal_edges->SetInputData(solid_data_);
    internal_edges->Update();

    prepared_ = std::make_shared<MeshDataSnapshot>();
    prepared_->source = source_;
    prepared_->model_data = model_data_;
    prepared_->original_faces = publishPolyData(face_data_);
    prepared_->original_edges = publishPolyData(edge_data_);
    prepared_->original_solids = vtkSmartPointer<vtkUnstructuredGrid>::New();
    prepared_->original_solids->ShallowCopy(solid_data_);
    prepared_->visible_faces = publishPolyData(faces);
    prepared_->visible_edges = publishPolyData(edges);
    prepared_->solid_surface = publishPolyData(surface->GetOutput());
    prepared_->solid_internal_edges = publishPolyData(internal_edges->GetOutput());
    dirty_ = false;
}

std::shared_ptr<const MeshDataSnapshot> MeshPipelineData::createSnapshot()
{
    assertOwnerThread();
    if (!source_)
        throw std::logic_error("MeshPipelineData has no source mesh");
    if (dirty_ || !prepared_)
        rebuildOutputs();
    auto snapshot = std::make_shared<MeshDataSnapshot>(*prepared_);
    snapshot->generation = generation_;
    snapshot->revision = ++revision_;
    snapshot->visible = visible_;
    snapshot->clip = clip_;
    return snapshot;
}
