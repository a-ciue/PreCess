#include "MeshActor.h"
#include "Core.h"
#include <vtkActor.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkPointData.h>
#include <vtkCompositePolyDataMapper.h>
#include <vtkDoubleArray.h>
#include <vtkMinimalStandardRandomSequence.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkNamedColors.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkPropAssembly.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkUnsignedCharArray.h>
#include <vtkUnstructuredGrid.h>  
#include <vtkGeometryFilter.h>
#include <vtkSMPTools.h>
#include <vtkExtractGeometry.h>
#include <vtkExtractPolyDataGeometry.h>
#include <vtkPlane.h>

vtkNew<vtkMinimalStandardRandomSequence> MeshActor::randomSequence;
vtkNew<vtkNamedColors> MeshActor::colors;

MeshActor::MeshActor(vtkRenderer* renderer, bool is_edge_render, bool is_vertex_render, ModelRenderMode render_mode)
    : renderer_(renderer)
    , render_mode_(render_mode)
    , edge_render_(is_edge_render)
{
    vtkNew<vtkNamedColors> colors;
    this->setRenderMode(render_mode);
    this->setRenderEdge(is_edge_render);
    this->setRenderVertex(is_vertex_render);

    this->edge_actor_->GetProperty()->SetLineWidth(2);
    this->vertex_actor_->GetProperty()->SetPointSize(3);
    this->vertex_actor_->GetProperty()->SetColor(colors->GetColor3d("Yellow").GetData());

    this->solid_actor_->SetMapper(solid_mapper_);
    this->face_actor_->SetMapper(face_mapper_);
    this->edge_actor_->SetMapper(edge_mapper_);
    this->vertex_actor_->SetMapper(vertex_mapper_);

    this->actor_->SetMapper(block_mapper_);
}

MeshActor::~MeshActor()
{
    if (this->renderer_) {
        renderer_->RemoveActor(this->actor_);
        renderer_->RemoveActor(this->solid_actor_);
        renderer_->RemoveActor(this->face_actor_);
        renderer_->RemoveActor(this->edge_actor_);
        renderer_->RemoveActor(this->vertex_actor_);
    }
}

void MeshActor::loadModelData(const MeshDataVtk& model_data)
{
    this->model_data_ = std::make_unique<MeshDataVtk>(model_data);

    // point data
    auto points_data = vtkSmartPointer<vtkPoints>::New();
    {
        auto& vtk_points = this->model_data_->vtk_points_;
        auto points_data_array = vtkSmartPointer<vtkDoubleArray>::New();

        points_data_array->SetNumberOfComponents(3);
        points_data_array->SetArray(const_cast<double*>(vtk_points.data()->data()), 3 * vtk_points.size(), 1);
        points_data->SetData(points_data_array);
    }

    // 添加原始点ID数组
    vtkNew<vtkIdTypeArray> originalPointIds;
    originalPointIds->SetNumberOfComponents(1);
    originalPointIds->SetName("vtkOriginalPointIds");
    originalPointIds->SetNumberOfTuples(points_data->GetNumberOfPoints());

    vtkSMPTools::For(0, points_data->GetNumberOfPoints(),
        [&](vtkIdType begin, vtkIdType end) {
            for (vtkIdType pointId = begin; pointId < end; ++pointId) {
                originalPointIds->SetValue(pointId, pointId);
            }
        });

    // vertex data
    vtkPolyData* vertex_poly = this->vertex_data_;
    {
        vtkNew<vtkCellArray> vertex_cells;
        vtkNew<vtkAOSDataArrayTemplate<Index>> index_array;
        vtkIdType size = points_data->GetNumberOfPoints();

        index_array->SetNumberOfValues(size);
        // 使用并行方式设置点id
        vtkSMPTools::For(0, size,
            [&](vtkIdType begin, vtkIdType end) {
                for (vtkIdType cellId = begin; cellId < end; ++cellId) {
                    index_array->SetValue(cellId, static_cast<Index>(cellId));
                }
            });
        vertex_cells->SetData(1, index_array);

        vertex_poly->SetPoints(points_data);
        vertex_poly->SetVerts(vertex_cells);

        vertex_poly->GetPointData()->AddArray(originalPointIds);
    }

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
        face_poly->SetPoints(points_data);
        face_poly->SetPolys(poly_data);

        face_poly->GetPointData()->AddArray(originalPointIds);
    }

    // edge data
    vtkPolyData* edge_poly = this->edge_data_;
    {
        vtkNew<vtkCellArray> edge_cells;
        vtkNew<vtkAOSDataArrayTemplate<Index>> index_array;
        auto& vtk_indices = this->model_data_->vtk_edge_cells_;
        index_array->SetArray(const_cast<Index*>(vtk_indices.data()), vtk_indices.size(), 1);
        edge_cells->SetData(2, index_array);

        edge_poly->SetPoints(points_data);
        edge_poly->SetLines(edge_cells);

        edge_poly->GetPointData()->AddArray(originalPointIds);
    }

    // solid data
    vtkUnstructuredGrid* solid_ugird = this->solid_data_;
    _createSolidUGird(*this->model_data_, *points_data, *solid_ugird);
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
    solid_ugird->GetPointData()->AddArray(originalPointIds);

    solid_filter_->SetInputData(solid_ugird);

    // mappers
    vertex_mapper_->SetInputData(vertex_poly);
    edge_mapper_->SetInputData(edge_poly);
    face_mapper_->SetInputData(face_poly);
    solid_mapper_->SetInputConnection(solid_filter_->GetOutputPort());

    createBlockMapper(*this->model_data_);
}

void MeshActor::setVisibility(bool visibility)
{
    this->visibility_ = visibility;
    this->actor_->SetVisibility(visibility);
    this->solid_actor_->SetVisibility(visibility);
    this->face_actor_->SetVisibility(visibility);
    this->edge_actor_->SetVisibility(visibility && this->edge_render_);
    this->vertex_actor_->SetVisibility(visibility && this->vertex_render_);
}

void MeshActor::setClipPlane(vtkPlane* plane)
{
    if (plane) {
        if (!clip_plane_) {
            solid_clipper_->SetInputData(this->solid_data_);
            face_clipper_->SetInputData(this->face_data_);
            edge_clipper_->SetInputData(this->edge_data_);
            vertex_clipper_->SetInputData(this->vertex_data_);

            solid_filter_->SetInputConnection(solid_clipper_->GetOutputPort());
            face_mapper_->SetInputConnection(face_clipper_->GetOutputPort());
            edge_mapper_->SetInputConnection(edge_clipper_->GetOutputPort());
            vertex_mapper_->SetInputConnection(vertex_clipper_->GetOutputPort());
        }
        solid_clipper_->SetImplicitFunction(plane);
        face_clipper_->SetImplicitFunction(plane);
        edge_clipper_->SetImplicitFunction(plane);
        vertex_clipper_->SetImplicitFunction(plane);
    } else {
        solid_filter_->SetInputData(this->solid_data_);
        face_mapper_->SetInputData(this->face_data_);
        edge_mapper_->SetInputData(this->edge_data_);
        vertex_mapper_->SetInputData(this->vertex_data_);
    }
    clip_plane_ = plane;
}

void MeshActor::setRenderEdge(bool is_render)
{
    this->edge_render_ = is_render;
    this->actor_->GetProperty()->SetEdgeVisibility(is_render);
    this->solid_actor_->GetProperty()->SetEdgeVisibility(is_render);
    this->face_actor_->GetProperty()->SetEdgeVisibility(is_render);
    this->edge_actor_->SetVisibility(is_render && this->visibility_);
}

void MeshActor::setRenderVertex(bool is_render)
{
    this->vertex_render_ = is_render;
    this->vertex_actor_->SetVisibility(is_render && this->visibility_);
}

void MeshActor::setRenderMode(ModelRenderMode render_mode)
{
    this->render_mode_ = render_mode;
    if (render_mode_ == ModelRenderMode::Face) {
        this->renderer_->RemoveActor(this->actor_);
        this->renderer_->AddActor(this->solid_actor_);
        this->renderer_->AddActor(this->face_actor_);
        this->renderer_->AddActor(this->edge_actor_);
        this->renderer_->AddActor(this->vertex_actor_);
    } else if (render_mode_ == ModelRenderMode::Block) {
        this->renderer_->RemoveActor(this->solid_actor_);
        this->renderer_->RemoveActor(this->face_actor_);
        this->renderer_->RemoveActor(this->edge_actor_);
        this->renderer_->RemoveActor(this->vertex_actor_);
        this->renderer_->AddActor(this->actor_);
    } else {
        std::cerr << "invalid renderMode in QRenderWindow::changeRenderer" << std::endl;
        return;
    }
}

bool MeshActor::getIsEdgeRender()
{
    return this->edge_render_;
}

bool MeshActor::getIsVertexRender()
{
    return this->vertex_render_;
}

ModelRenderMode MeshActor::getMeshRenderMode()
{
    return this->render_mode_;
}

void MeshActor::createBlockMapper(const MeshDataVtk& model_data)
{
    auto multiblock = vtkSmartPointer<vtkMultiBlockDataSet>::New();
    const auto& blocks = model_data.model_blocks_->block_datas;

    for (size_t block_index = 0; block_index < blocks.size(); ++block_index) {
        const auto& block = blocks[block_index];

        std::unordered_map<vtkIdType, vtkIdType> global_to_local;
        auto points = vtkSmartPointer<vtkPoints>::New();
        auto cells = vtkSmartPointer<vtkCellArray>::New();
        auto grid = vtkSmartPointer<vtkPolyData>::New();

        auto colors = vtkSmartPointer<vtkUnsignedCharArray>::New();
        colors->SetNumberOfComponents(3);
        colors->SetName("BlockColors");

        // 为该 block 随机生成颜色
        const std::array<unsigned char, 3> rgb = {
            static_cast<unsigned char>(rand() % 256),
            static_cast<unsigned char>(rand() % 256),
            static_cast<unsigned char>(rand() % 256)
        };

        vtkIdType local_id = 0;
        points->Allocate(static_cast<vtkIdType>(block.faces_.size() * 3)); // 预分配，粗略估计
        cells->AllocateEstimate(block.faces_.size(), 4); // 预估每个 cell 是三角形

        for (vtkIdType face_id : block.faces_) {
            Index offset = model_data.vtk_face_cells_offset_[face_id],
                  offset_to = model_data.vtk_face_cells_offset_[face_id + 1];
            const Index* index_begin = model_data.vtk_face_cells_.data() + offset;
            std::vector<vtkIdType> tri_pts(offset_to - offset);

            for (Index i = 0; i < tri_pts.size(); i++) {
                vtkIdType global_id = index_begin[i];
                auto iter = global_to_local.find(global_id);
                if (iter == global_to_local.end()) {
                    const auto& pt = model_data.vtk_points_[global_id];
                    points->InsertPoint(local_id, pt.data());
                    global_to_local[global_id] = local_id;
                    tri_pts[i] = local_id++;
                } else {
                    tri_pts[i] = iter->second;
                }
            }

            cells->InsertNextCell(tri_pts.size(), tri_pts.data());
            colors->InsertNextTypedTuple(rgb.data());
        }

        grid->SetPoints(points);
        grid->SetPolys(cells);
        grid->GetCellData()->SetScalars(colors);

        multiblock->SetBlock(static_cast<unsigned int>(block_index), grid);
    }

    this->block_mapper_->SetInputDataObject(multiblock);
    this->block_mapper_->SetScalarModeToUseCellData();
    this->block_mapper_->ScalarVisibilityOn();
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
