#include "MeshPipelineData.h"
#include "FaceHideFilter.h"
#include "MeshData.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <vtkAOSDataArrayTemplate.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkDoubleArray.h>
#include <vtkExtractCells.h>
#include <vtkExtractEdges.h>
#include <vtkExtractGeometry.h>
#include <vtkExtractPolyDataGeometry.h>
#include <vtkGeometryFilter.h>
#include <vtkIdList.h>
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

bool MeshDataSnapshot::hasSameScalarCells(const MeshDataSnapshot& other) const
{
    if (generation != other.generation)
        return false;
    const auto same_ids = [](vtkIdTypeArray* left, vtkIdTypeArray* right, vtkIdType source_count) {
        if (!left || !right || left->GetNumberOfValues() != right->GetNumberOfValues())
            return false;
        const auto count = left->GetNumberOfValues();
        if (left == right || count == 0 || std::equal(left->GetPointer(0), left->GetPointer(0) + count, right->GetPointer(0)))
            return true;
        // 原始 ID 在每个结果中唯一；只有顺序不同时才用临时位图比较集合。
        std::vector<bool> retained(source_count, false);
        for (vtkIdType i = 0; i < count; ++i) {
            const auto id = left->GetValue(i);
            if (id < 0 || id >= source_count)
                return false;
            retained[id] = true;
        }
        for (vtkIdType i = 0; i < count; ++i) {
            const auto id = right->GetValue(i);
            if (id < 0 || id >= source_count || !retained[id])
                return false;
        }
        return true;
    };
    const auto same_cells = [&same_ids](vtkPolyData* left, vtkPolyData* right, const char* name, vtkIdType source_count) {
        if (left == right)
            return true;
        if (left->GetNumberOfCells() != right->GetNumberOfCells())
            return false;
        return left->GetNumberOfCells() == 0
            || same_ids(vtkIdTypeArray::SafeDownCast(left->GetCellData()->GetArray(name)),
                vtkIdTypeArray::SafeDownCast(right->GetCellData()->GetArray(name)), source_count);
    };
    return same_cells(visible_faces, other.visible_faces, "PrecessFaceIds", original_faces->GetNumberOfCells())
        && same_cells(visible_edges, other.visible_edges, "vtkOriginalCellIds", original_edges->GetNumberOfCells())
        && same_ids(visible_solid_ids, other.visible_solid_ids, original_solids->GetNumberOfCells());
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
    scalar_ranges_.clear();
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
        // 重合边也有独立身份，不能以端点坐标判断是否为同一个原始单元。
        vtkNew<vtkIdTypeArray> edge_ids;
        edge_ids->SetName("vtkOriginalCellIds");
        edge_ids->SetNumberOfTuples(edge_poly->GetNumberOfCells());
        for (vtkIdType id = 0; id < edge_poly->GetNumberOfCells(); ++id)
            edge_ids->SetValue(id, id);
        edge_poly->GetCellData()->AddArray(edge_ids);
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
    // 源数据更新只校验当前配置；属性失效清空配置，单独失效的筛选不影响着色。
    if (scalar_config_) {
        if (scalarAttributeComponents(scalar_config_->attribute, scalar_config_->type) == 0)
            scalar_config_.reset();
        else if (scalar_config_->display && !scalarRange(scalar_config_->attribute, scalar_config_->type))
            scalar_config_->display.reset();
    }
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
    // 控件可能重复发布同一平面，避免重算不变的裁剪输出。
    if (clip_.enabled == plane.enabled && clip_.origin == plane.origin && clip_.normal == plane.normal)
        return;
    clip_ = plane;
    dirty_ = true;
}

vtkDataArray* MeshPipelineData::scalarValues(const std::string& attribute, ElementType type) const
{
    switch (type) {
    case ElementType::VERTEX:
        return face_data_ ? face_data_->GetPointData()->GetArray(attribute.c_str()) : nullptr;
    case ElementType::EDGE:
        return edge_data_ ? edge_data_->GetCellData()->GetArray(attribute.c_str()) : nullptr;
    case ElementType::FACE:
        return face_data_ ? face_data_->GetCellData()->GetArray(attribute.c_str()) : nullptr;
    case ElementType::SOLID:
        return solid_data_ ? solid_data_->GetCellData()->GetArray(attribute.c_str()) : nullptr;
    }
    return nullptr;
}

int MeshPipelineData::scalarAttributeComponents(const std::string& attribute, ElementType type) const
{
    assertOwnerThread();
    auto* values = scalarValues(attribute, type);
    return values ? values->GetNumberOfComponents() : 0;
}

std::optional<std::array<double, 2>> MeshPipelineData::scalarRange(const std::string& attribute, ElementType type)
{
    assertOwnerThread();
    const auto key = std::make_pair(attribute, type);
    if (const auto found = scalar_ranges_.find(key); found != scalar_ranges_.end())
        return found->second;
    auto* values = scalarValues(attribute, type);
    std::optional<std::array<double, 2>> range;
    if (values && values->GetNumberOfComponents() == 1) {
        double lower = std::numeric_limits<double>::infinity();
        double upper = -lower;
        // 统计完整原始数组，缺失值不参与值域计算。
        for (vtkIdType id = 0; id < values->GetNumberOfTuples(); ++id) {
            const double value = values->GetComponent(id, 0);
            if (std::isfinite(value)) {
                lower = std::min(lower, value);
                upper = std::max(upper, value);
            }
        }
        if (lower <= upper)
            range = std::array<double, 2> { lower, upper };
    }
    scalar_ranges_.emplace(key, range);
    return range;
}

void MeshPipelineData::setScalarConfig(std::optional<ScalarDisplayConfig> config)
{
    assertOwnerThread();
    if (config) {
        const auto valid_range = [](const auto& range) {
            return !range || (std::isfinite((*range)[0]) && std::isfinite((*range)[1]) && (*range)[0] <= (*range)[1]);
        };
        if (static_cast<int>(config->type) < 0 || static_cast<int>(config->type) > 3
            || scalarAttributeComponents(config->attribute, config->type) == 0
            || !valid_range(config->mapping) || !valid_range(config->display)
            || (config->display && (config->type == ElementType::VERTEX || !scalarRange(config->attribute, config->type))))
            throw std::invalid_argument("标量显示配置或属性身份无效");
        // 缺省映射在接受配置时解析，之后源数据更新仍保留本次采用的映射范围。
        if (!config->mapping)
            config->mapping = scalarRange(config->attribute, config->type);
    }
    const auto before = scalar_config_ ? scalar_config_->display : std::nullopt;
    const auto after = config ? config->display : std::nullopt;
    // 无筛选时切换属性或映射不改变几何；有筛选时身份和区间共同决定输出。
    const bool filter_changed = before != after || (after && (!scalar_config_ || scalar_config_->attribute != config->attribute || scalar_config_->type != config->type));
    scalar_config_ = std::move(config);
    dirty_ = dirty_ || filter_changed;
}

MeshSnapshotUpdate MeshPipelineData::createScalarUpdate()
{
    const auto previous = prepared_;
    const bool rebuilt = dirty_ || !prepared_;
    auto snapshot = createSnapshot();
    const bool geometry_changed = !previous || previous->generation != snapshot->generation
        || (rebuilt && !previous->hasSameScalarCells(*snapshot));
    return { std::move(snapshot), geometry_changed };
}

bool MeshPipelineData::setScalarDisplayRange(const std::string& attribute, ElementType type,
    bool enabled, const std::array<double, 2>& range)
{
    assertOwnerThread();
    if (!enabled)
        return clearScalarDisplayRange();
    const bool same = scalar_config_ && scalar_config_->attribute == attribute && scalar_config_->type == type;
    if (same && scalar_config_->display == range)
        return false;
    setScalarConfig(ScalarDisplayConfig { attribute, type, same ? scalar_config_->mapping : std::nullopt, range });
    return true;
}

bool MeshPipelineData::clearScalarDisplayRange()
{
    assertOwnerThread();
    if (!scalar_config_ || !scalar_config_->display)
        return false;
    scalar_config_->display.reset();
    dirty_ = true;
    return true;
}

void MeshPipelineData::rebuildOutputs()
{
    // 每次重建使用新的 Filter，下一次 Update 不会改写已发布快照引用的输出数组。
    vtkSmartPointer<vtkPolyData> range_faces = face_data_;
    vtkSmartPointer<vtkPolyData> range_edges = edge_data_;
    vtkSmartPointer<vtkUnstructuredGrid> range_solids = solid_data_;
    if (scalar_config_ && scalar_config_->display) {
        const auto& config = *scalar_config_;
        auto* values = scalarValues(config.attribute, config.type);
        vtkDataSet* input = config.type == ElementType::EDGE ? static_cast<vtkDataSet*>(edge_data_)
            : config.type == ElementType::FACE               ? static_cast<vtkDataSet*>(face_data_)
                                                             : static_cast<vtkDataSet*>(solid_data_);
        vtkNew<vtkIdList> ids;
        // 按原始单元顺序提取闭区间，原始 ID 和属性随提取结果透传。
        for (vtkIdType id = 0; id < values->GetNumberOfTuples(); ++id) {
            const double value = values->GetComponent(id, 0);
            if (std::isfinite(value) && value >= (*config.display)[0] && value <= (*config.display)[1])
                ids->InsertNextId(id);
        }
        vtkNew<vtkExtractCells> extract;
        extract->SetInputData(input);
        extract->SetCellList(ids);
        extract->AssumeSortedAndUniqueIdsOn();
        extract->PassThroughCellIdsOff();
        extract->Update();
        if (config.type == ElementType::SOLID) {
            range_solids = vtkSmartPointer<vtkUnstructuredGrid>::New();
            range_solids->ShallowCopy(extract->GetOutput());
        } else {
            vtkNew<vtkGeometryFilter> geometry;
            geometry->SetInputData(extract->GetOutput());
            geometry->Update();
            if (config.type == ElementType::FACE)
                range_faces = publishPolyData(geometry->GetOutput());
            else
                range_edges = publishPolyData(geometry->GetOutput());
        }
    }
    vtkNew<FaceHideFilter> hider;
    vtkNew<vtkIdTypeArray> hidden_ids;
    // 未压缩的面直接沿用原始隐藏 ID，避免原有显隐和裁剪路径扫描全部面。
    if (range_faces == face_data_) {
        for (auto id : hidden_face_ids_)
            hidden_ids->InsertNextValue(id);
    } else if (!hidden_face_ids_.empty()) {
        auto* original_ids = vtkIdTypeArray::SafeDownCast(range_faces->GetCellData()->GetArray("PrecessFaceIds"));
        // 仅在面筛选压缩下标且存在人工隐藏时，按原始身份重映射。
        for (vtkIdType id = 0; id < range_faces->GetNumberOfCells(); ++id) {
            const auto original_id = original_ids ? original_ids->GetValue(id) : id;
            if (std::binary_search(hidden_face_ids_.begin(), hidden_face_ids_.end(), original_id))
                hidden_ids->InsertNextValue(id);
        }
    }
    hider->SetInputData(range_faces);
    hider->SetHiddenCellIds(hidden_ids);
    hider->Update();

    vtkNew<vtkPlane> plane;
    plane->SetOrigin(clip_.origin.data());
    plane->SetNormal(clip_.normal.data());
    vtkNew<vtkExtractPolyDataGeometry> face_clipper;
    vtkNew<vtkExtractPolyDataGeometry> edge_clipper;
    vtkNew<vtkExtractGeometry> solid_clipper;
    vtkPolyData* faces = hider->GetOutput();
    vtkPolyData* edges = range_edges;
    vtkUnstructuredGrid* solids = range_solids;
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
    // 内部线框始终来自完整原始体，与裁剪及面隐藏无关；同一源数据只提取一次。
    // 复用已发布的只读输出，源网格重新加载（含坐标/属性更新）后才重新提边。
    vtkSmartPointer<vtkPolyData> internal_edges;
    if (prepared_ && prepared_->source == source_) {
        internal_edges = prepared_->solid_internal_edges;
    } else {
        vtkNew<vtkExtractEdges> extractor;
        extractor->SetInputData(solid_data_);
        // 线框沿用组件点身份，无需按坐标合并；保留原点编号也能避免大体网格的空间查找瓶颈。
        extractor->UseAllPointsOn();
        extractor->Update();
        internal_edges = publishPolyData(extractor->GetOutput());
    }

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
    prepared_->visible_solid_ids = vtkIdTypeArray::SafeDownCast(solids->GetCellData()->GetArray("vtkOriginalCellIds"));
    // VTK 的空输出可能省略数组，空身份数组仍明确表示零个保留单元。
    if (!prepared_->visible_solid_ids && solids->GetNumberOfCells() == 0)
        prepared_->visible_solid_ids = vtkSmartPointer<vtkIdTypeArray>::New();
    prepared_->solid_internal_edges = internal_edges;
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
    snapshot->scalar_config = scalar_config_;
    // 现有输出缓存同时保存最新完成配置；旧快照只读保留，不原地回写。
    prepared_ = snapshot;
    return snapshot;
}
