#include "MeshActor.h"
#include "renderStrategy/AttributeOperator.h"
#include "Core.h"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <vtkActor.h>
#include <vtkCellArray.h>
#include <vtkCellCenters.h>
#include <vtkCellData.h>
#include <vtkDoubleArray.h>
#include <vtkExtractGeometry.h>
#include <vtkExtractPolyDataGeometry.h>
#include <vtkGeometryFilter.h>
#include <vtkMinimalStandardRandomSequence.h>
#include <vtkNamedColors.h>
#include <vtkPlane.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkPropAssembly.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSMPTools.h>
#include <vtkUnstructuredGrid.h>
vtkNew<vtkMinimalStandardRandomSequence> MeshActor::randomSequence;
vtkNew<vtkNamedColors> MeshActor::colors;

namespace {
// 将组件局部点属性写入全局 PointData，供面/体 mapper 按点属性渲染。
void addPointAttributes(
    vtkPointData& point_data,
    const std::map<std::string, std::vector<double>>& attributes,
    vtkIdType global_point_count,
    const std::vector<Index>& local_to_global)
{
    // 全局点池未同步时不能挂载点属性。
    if (global_point_count <= 0)
        return;

    // component 重构后，点属性仍是组件局部数组，必须依赖局部点到全局点的映射。
    if (local_to_global.empty())
        return;

    const size_t local_point_count = local_to_global.size();
    for (const auto& [attr_name, attr_values] : attributes) {
        if (attr_values.empty())
            continue;

        // 属性数组按 [point0 分量..., point1 分量...] 存放，总长度必须是点数的整数倍。
        if (attr_values.size() % local_point_count != 0) {
            spdlog::error("Attribute {} size mismatch: {} values for {} points",
                attr_name, attr_values.size(), local_point_count);
            continue;
        }

        // VTK PointData 挂在共享的全局点池上，tuple 数量必须按全局点数创建。
        const size_t ncomp = attr_values.size() / local_point_count;
        auto array = vtkSmartPointer<vtkDoubleArray>::New();
        array->SetNumberOfComponents(static_cast<int>(ncomp));
        array->SetName(attr_name.c_str());
        array->SetNumberOfTuples(global_point_count);
        // 非本组件的点也需要占位，否则 tuple id 无法和全局点 id 对齐。
        for (int comp = 0; comp < static_cast<int>(ncomp); ++comp)
            array->FillComponent(comp, 0.0);

        // 将组件局部第 i 个点属性写到 local_to_global[i] 指向的全局 tuple 上。
        for (size_t i = 0; i < local_point_count; ++i) {
            const Index global_point_id = local_to_global[i];
            if (global_point_id < 0 || global_point_id >= static_cast<Index>(global_point_count)) {
                spdlog::error("Attribute {} point id {} exceeds global point count {}",
                    attr_name, global_point_id, global_point_count);
                continue;
            }
            const vtkIdType tuple_id = static_cast<vtkIdType>(global_point_id);
            array->SetTuple(tuple_id, &attr_values[i * ncomp]);
        }
        point_data.AddArray(array);
    }
}

// 将面属性或体属性写入对应 VTK 数据对象的 CellData。
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
}

MeshActor::MeshActor(
    vtkRenderer* renderer,
    vtkPoints* global_points)
    : renderer_(renderer)
    , global_points_(global_points)
{
    if (!global_points_) {
        throw std::invalid_argument("MeshActor: global_points cannot be null");
    }
    vtkNew<vtkNamedColors> colors;

    this->renderer_->AddActor(this->solid_actor_);
    this->renderer_->AddActor(this->face_actor_);
    this->renderer_->AddActor(this->edge_actor_);
    this->renderer_->AddActor(this->glyph3D_actor_);

    this->edge_actor_->GetProperty()->SetLineWidth(2);

    this->solid_actor_->SetMapper(solid_mapper_);
    this->face_actor_->SetMapper(face_mapper_);
    this->edge_actor_->SetMapper(edge_mapper_);
    this->glyph3D_actor_->SetMapper(glyph3D_mapper_);
}

MeshActor::~MeshActor()
{
    if (this->renderer_) {
        renderer_->RemoveActor(this->solid_actor_);
        renderer_->RemoveActor(this->face_actor_);
        renderer_->RemoveActor(this->edge_actor_);
        renderer_->RemoveActor(this->glyph3D_actor_);
    }
}

void MeshActor::loadModelData(const MeshDataVtk& model_data)
{
    ensureOriginalPointIds();

    this->model_data_ = std::make_unique<MeshDataVtk>(model_data);

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
        face_poly->SetPoints(global_points_);
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
        addPointAttributes(*face_poly->GetPointData(), model_data.vertex_attributes_,
            global_points_->GetNumberOfPoints(), model_data.local_to_global_);
        addCellAttributes(*face_poly->GetCellData(), model_data.face_attributes_,
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

        edge_poly->SetPoints(global_points_);
        edge_poly->SetLines(edge_cells);

        edge_poly->GetPointData()->AddArray(original_point_ids_.GetPointer());
    }

    // solid data
    vtkUnstructuredGrid* solid_ugird = this->solid_data_;
    _createSolidUGird(*this->model_data_, *global_points_, *solid_ugird);
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
    addPointAttributes(*solid_ugird->GetPointData(), model_data.vertex_attributes_,
        global_points_->GetNumberOfPoints(), model_data.local_to_global_);
    addCellAttributes(*solid_ugird->GetCellData(), model_data.solid_attributes_, solid_cells_count, "solid");

    // 单元中心点只和几何拓扑有关，在加载数据时统一计算并缓存，属性渲染阶段直接复用。 
    {
        vtkNew<vtkCellCenters> face_centers;
        face_centers->SetInputData(face_poly);
        face_centers->Update();
        face_cell_centers_->DeepCopy(face_centers->GetOutput());

        vtkNew<vtkCellCenters> solid_centers;
        solid_centers->SetInputData(solid_ugird);
        solid_centers->Update();
        solid_cell_centers_->DeepCopy(solid_centers->GetOutput());
    }

    solid_filter_->SetInputData(solid_ugird);
    solid_edge_extractor_->SetInputData(solid_ugird);

    // mappers
    edge_mapper_->SetInputData(edge_poly);
    face_mapper_->SetInputData(face_poly);
    face_mapper_->Modified(); // 确保多边形网格被正确渲染
    solid_mapper_->SetInputConnection(solid_filter_->GetOutputPort());

    edge_mapper_->SetScalarVisibility(0);
    face_mapper_->SetScalarVisibility(0);
    solid_mapper_->SetScalarVisibility(0);
}

void MeshActor::setVisibility(bool visibility)
{
    this->visibility_ = visibility;
    applyStyle();
}

bool MeshActor::isVisible() const
{ 
    return visibility_ && style_ != MeshRenderStyle::Hidden; 
}

void MeshActor::setClipPlane(vtkPlane* plane)
{
    if (plane) {
        if (!clip_plane_) {
            solid_clipper_->SetInputData(this->solid_data_);
            face_clipper_->SetInputData(this->face_data_);
            edge_clipper_->SetInputData(this->edge_data_);

            solid_filter_->SetInputConnection(solid_clipper_->GetOutputPort());
            face_mapper_->SetInputConnection(face_clipper_->GetOutputPort());
            edge_mapper_->SetInputConnection(edge_clipper_->GetOutputPort());
        }
        solid_clipper_->SetImplicitFunction(plane);
        face_clipper_->SetImplicitFunction(plane);
        edge_clipper_->SetImplicitFunction(plane);
    } else {
        solid_filter_->SetInputData(this->solid_data_);
        face_mapper_->SetInputData(this->face_data_);
        edge_mapper_->SetInputData(this->edge_data_);
    }
    clip_plane_ = plane;
}

void MeshActor::setRenderStyle(MeshRenderStyle style)
{
    this->style_ = style;
    applyStyle();
}

MeshRenderStyle MeshActor::getRenderStyle() const
{
    return style_;
}

void MeshActor::applyStyle()
{
    if (style_ == MeshRenderStyle::Hidden || !visibility_) {
        solid_actor_->SetVisibility(false);
        face_actor_->SetVisibility(false);
        edge_actor_->SetVisibility(false);
        glyph3D_actor_->SetVisibility(false);
        return;
    }

    glyph3D_actor_->SetVisibility(true);

    if (style_ == MeshRenderStyle::WireframeInternal) {
        solid_mapper_->SetInputConnection(solid_edge_extractor_->GetOutputPort());
    } else {
        solid_mapper_->SetInputConnection(solid_filter_->GetOutputPort());
    }

    switch (style_) {
    case MeshRenderStyle::FaceWithEdges:
        solid_actor_->SetVisibility(true);
        solid_actor_->GetProperty()->SetRepresentationToSurface();
        solid_actor_->GetProperty()->SetEdgeVisibility(true);
        solid_actor_->GetProperty()->SetOpacity(1.0);
        face_actor_->SetVisibility(true);
        face_actor_->GetProperty()->SetRepresentationToSurface();
        face_actor_->GetProperty()->SetEdgeVisibility(true);
        face_actor_->GetProperty()->SetOpacity(1.0);
        edge_actor_->SetVisibility(this->visibility_);
        break;
    case MeshRenderStyle::Face:
        solid_actor_->SetVisibility(true);
        solid_actor_->GetProperty()->SetRepresentationToSurface();
        solid_actor_->GetProperty()->SetEdgeVisibility(false);
        solid_actor_->GetProperty()->SetOpacity(1.0);
        face_actor_->SetVisibility(true);
        face_actor_->GetProperty()->SetRepresentationToSurface();
        face_actor_->GetProperty()->SetEdgeVisibility(false);
        face_actor_->GetProperty()->SetOpacity(1.0);
        edge_actor_->SetVisibility(false);
        break;
    case MeshRenderStyle::Transparent75:
        solid_actor_->SetVisibility(true);
        solid_actor_->GetProperty()->SetRepresentationToSurface();
        solid_actor_->GetProperty()->SetEdgeVisibility(true);
        solid_actor_->GetProperty()->SetOpacity(0.75);
        face_actor_->SetVisibility(true);
        face_actor_->GetProperty()->SetRepresentationToSurface();
        face_actor_->GetProperty()->SetEdgeVisibility(true);
        face_actor_->GetProperty()->SetOpacity(0.75);
        edge_actor_->SetVisibility(this->visibility_);
        break;
    case MeshRenderStyle::Transparent50:
        solid_actor_->SetVisibility(true);
        solid_actor_->GetProperty()->SetRepresentationToSurface();
        solid_actor_->GetProperty()->SetEdgeVisibility(true);
        solid_actor_->GetProperty()->SetOpacity(0.50);
        face_actor_->SetVisibility(true);
        face_actor_->GetProperty()->SetRepresentationToSurface();
        face_actor_->GetProperty()->SetEdgeVisibility(true);
        face_actor_->GetProperty()->SetOpacity(0.50);
        edge_actor_->SetVisibility(this->visibility_);
        break;
    case MeshRenderStyle::Transparent25:
        solid_actor_->SetVisibility(true);
        solid_actor_->GetProperty()->SetRepresentationToSurface();
        solid_actor_->GetProperty()->SetEdgeVisibility(true);
        solid_actor_->GetProperty()->SetOpacity(0.25);
        face_actor_->SetVisibility(true);
        face_actor_->GetProperty()->SetRepresentationToSurface();
        face_actor_->GetProperty()->SetEdgeVisibility(true);
        face_actor_->GetProperty()->SetOpacity(0.25);
        edge_actor_->SetVisibility(this->visibility_);
        break;
    case MeshRenderStyle::WireframeInternal:
        solid_actor_->SetVisibility(true);
        solid_actor_->GetProperty()->SetRepresentationToWireframe();
        solid_actor_->GetProperty()->SetOpacity(1.0);
        face_actor_->SetVisibility(true);
        face_actor_->GetProperty()->SetRepresentationToWireframe();
        face_actor_->GetProperty()->SetOpacity(1.0);
        edge_actor_->SetVisibility(this->visibility_);
        break;
    case MeshRenderStyle::WireframeSurface:
        solid_actor_->SetVisibility(true);
        solid_actor_->GetProperty()->SetRepresentationToWireframe();
        solid_actor_->GetProperty()->SetOpacity(1.0);
        face_actor_->SetVisibility(true);
        face_actor_->GetProperty()->SetRepresentationToWireframe();
        face_actor_->GetProperty()->SetOpacity(1.0);
        edge_actor_->SetVisibility(false);
        break;
    default:
        break;
    }
}

void MeshActor::_createSolidUGird(const MeshDataVtk& model_data, vtkPoints& points, vtkUnstructuredGrid& solid_data)
{
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

void MeshActor::cancelActiveAttribute()
{
    if (render_strategy_) {
        AttributeOperator op(this);
        render_strategy_->cancelActiveAttribute(op);
    }
}

void MeshActor::setRenderStrategy(std::unique_ptr<IAttributeRenderStrategy> strategy)
{
    render_strategy_ = std::move(strategy);
}

void MeshActor::renderAttribute(
    const std::string& attr_name,
    std::map<std::string, std::any> args)
{
    if (render_strategy_) {
        AttributeOperator op(this);
        render_strategy_->render(op, attr_name, args);
    }
}

void MeshActor::ensureOriginalPointIds()
{
    vtkIdType n = global_points_->GetNumberOfPoints();
    original_point_ids_->SetName("vtkOriginalPointIds");
    original_point_ids_->SetNumberOfComponents(1);
    original_point_ids_->SetNumberOfTuples(n);
    vtkSMPTools::For(0, n,
        [&](vtkIdType begin, vtkIdType end) {
            for (vtkIdType i = begin; i < end; ++i) {
                original_point_ids_->SetValue(i, i);
            }
        });
    original_point_ids_->Modified();
}
