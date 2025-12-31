#include "MeshActor.h"
#include "Core.h"
#include <spdlog/spdlog.h>
#include <vtkActor.h>
#include <vtkAlgorithmOutput.h>
#include <vtkArrowSource.h>
#include <vtkCellArray.h>
#include <vtkCellCenters.h>
#include <vtkCellData.h>
#include <vtkCellDataToPointData.h>
#include <vtkCompositePolyDataMapper.h>
#include <vtkDoubleArray.h>
#include <vtkExtractGeometry.h>
#include <vtkExtractPolyDataGeometry.h>
#include <vtkGeometryFilter.h>
#include <vtkGlyph3D.h>
#include <vtkImageReader2.h>
#include <vtkImageReader2Factory.h>
#include <vtkLookupTable.h>
#include <vtkMinimalStandardRandomSequence.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkNamedColors.h>
#include <vtkPlane.h>
#include <vtkPointData.h>
#include <vtkPointDataToCellData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkPropAssembly.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSMPTools.h>
#include <vtkTexture.h>
#include <vtkTextureMapToPlane.h>
#include <vtkUnsignedCharArray.h>
#include <vtkUnstructuredGrid.h>

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
    this->glyph3D_actor_->SetMapper(glyph3D_mapper_);

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
        renderer_->RemoveActor(this->glyph3D_actor_);
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

        // 动态处理顶点属性
        const size_t num_vertex = size;
        for (const auto& attr : model_data.vertex_attributes_) {
            const std::string& attr_name = attr.first;
            const std::vector<double>& attr_values = attr.second;
            bool isTriple = attr_name.substr(attr_name.size() - 2) == "_3";
            bool isDouble = attr_name.substr(attr_name.size() - 2) == "_2";
            if (attr_values.size() < num_vertex) {
                spdlog::error("Attribute {} has insufficient values", attr_name);
                continue;
            }
            if (!isTriple && !isDouble) {
                // 单分量属性 (如 "pressure")
                auto scalar_array = vtkSmartPointer<vtkDoubleArray>::New();
                scalar_array->SetNumberOfComponents(1);
                scalar_array->SetName(attr_name.c_str());

                for (size_t i = 0; i < num_vertex; ++i) {
                    scalar_array->InsertNextValue(attr_values[i]);
                }
                vertex_poly->GetPointData()->AddArray(scalar_array);
            } else if (isDouble) {
                // 双元组属性 (如 "vertex_uv_2")
                auto vector_array = vtkSmartPointer<vtkDoubleArray>::New();
                vector_array->SetNumberOfComponents(2);
                vector_array->SetName(attr_name.c_str());

                for (size_t i = 0; i < num_vertex; ++i) {
                    vector_array->InsertNextTuple2(
                        attr_values[i * 2],
                        attr_values[i * 2 + 1]);
                }
                vertex_poly->GetPointData()->AddArray(vector_array);
            } else if (isTriple) {
                // 三元组属性 (如 "vertex_vector_3")
                auto vector_array = vtkSmartPointer<vtkDoubleArray>::New();
                vector_array->SetNumberOfComponents(3);
                vector_array->SetName(attr_name.c_str());

                for (size_t i = 0; i < num_vertex; ++i) {
                    vector_array->InsertNextTuple3(
                        attr_values[i * 3],
                        attr_values[i * 3 + 1],
                        attr_values[i * 3 + 2]);
                }
                vertex_poly->GetPointData()->AddArray(vector_array);
            } else {

                std::cerr << "Warning: Vertex attribute '" << attr_name
                          << std::endl;
            }
        }
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

        //  拷贝属性
        vtkPointData* src = this->vertex_data_->GetPointData();
        vtkPointData* dst = this->face_data_->GetPointData();
        dst->ShallowCopy(src);
        // 新增：动态处理面属性
        const size_t num_faces = face_poly->GetNumberOfCells();
        for (const auto& attr : model_data.face_attributes_) {
            const std::string& attr_name = attr.first;
            const std::vector<double>& attr_values = attr.second;
            bool isTriple = !attr_name.empty() && attr_name.back() == '3';
            bool isDouble = !attr_name.empty() && attr_name.back() == '2';
            if (!isTriple && !isDouble) {
                // 单分量属性 (如 "face_pressure")
                auto scalar_array = vtkSmartPointer<vtkDoubleArray>::New();
                scalar_array->SetNumberOfComponents(1);
                scalar_array->SetName(attr_name.c_str());

                for (size_t i = 0; i < num_faces; ++i) {
                    scalar_array->InsertNextValue(attr_values[i]);
                }
                face_poly->GetCellData()->AddArray(scalar_array);
            } else if (isDouble) {
                // 双分量属性
                auto vector_array = vtkSmartPointer<vtkDoubleArray>::New();
                vector_array->SetNumberOfComponents(2);
                vector_array->SetName(attr_name.c_str());
                for (size_t i = 0; i < num_faces; ++i) {
                    vector_array->InsertNextTuple2(
                        attr_values[i * 2],
                        attr_values[i * 2 + 1]);
                }
                face_poly->GetCellData()->AddArray(vector_array);
            } else if (isTriple) {
                // 三分量属性
                auto vector_array = vtkSmartPointer<vtkDoubleArray>::New();
                vector_array->SetNumberOfComponents(3);
                vector_array->SetName(attr_name.c_str());

                for (size_t i = 0; i < num_faces; ++i) {
                    vector_array->InsertNextTuple3(
                        attr_values[i * 3],
                        attr_values[i * 3 + 1],
                        attr_values[i * 3 + 2]);
                }
                face_poly->GetCellData()->AddArray(vector_array);

            } else {
                std::cerr << "Warning: Face attribute '" << attr_name
                          << std::endl;
            }
        }
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

    vertex_mapper_->SetScalarVisibility(0);
    edge_mapper_->SetScalarVisibility(0);
    face_mapper_->SetScalarVisibility(0);
    solid_mapper_->SetScalarVisibility(0);
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
    this->glyph3D_actor_->SetVisibility(visibility);
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
        this->renderer_->AddActor(this->glyph3D_actor_);
    } else if (render_mode_ == ModelRenderMode::Block) {
        this->renderer_->RemoveActor(this->solid_actor_);
        this->renderer_->RemoveActor(this->face_actor_);
        this->renderer_->RemoveActor(this->edge_actor_);
        this->renderer_->RemoveActor(this->vertex_actor_);
        this->renderer_->RemoveActor(this->glyph3D_actor_);
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

void MeshActor::setActiveScalarAttribute(std::string attr_name, ElementType type)
{
    switch (type) {
    case VERTEX: {
        vtkDataArray* array = this->vertex_data_->GetPointData()->GetArray(attr_name.c_str());
        this->vertex_data_->GetPointData()->SetActiveAttribute(attr_name.c_str(), vtkDataSetAttributes::SCALARS);
        // 设置映射范围
        double range[2];
        array->GetRange(range);
        vertex_mapper_->SetScalarRange(range[0], range[1]);
        vertex_mapper_->SetScalarVisibility(1);
        vertex_mapper_->SetColorModeToMapScalars(); // 使用 colormap
        if (this->face_data_) {
            this->face_data_->GetPointData()->SetActiveScalars(attr_name.c_str());
            face_mapper_->SetScalarModeToUsePointData();
            face_mapper_->SetScalarVisibility(1);
            face_mapper_->SetScalarRange(range[0], range[1]);
            face_mapper_->SetColorModeToMapScalars();
        }
    } break;
    case FACE: {
        vtkDataArray* array = this->face_data_->GetCellData()->GetArray(attr_name.c_str());
        // 设置映射范围
        double range[2];
        array->GetRange(range);
        face_mapper_->SetScalarRange(range[0], range[1]);
        face_mapper_->SetScalarModeToUseCellData();
        this->face_data_->GetCellData()->SetActiveScalars(attr_name.c_str());
        face_mapper_->SetScalarVisibility(1);
        face_mapper_->SetColorModeToMapScalars(); // 使用 colormap
    } break;
    case EDGE: {

    } break;
    }
}

void MeshActor::setActiveVectorAttribute(std::string attr_name, ElementType type)
{
    switch (type) {
        {
        case VERTEX:
            vtkDataArray* array = this->vertex_data_->GetPointData()->GetArray(attr_name.c_str());
            this->vertex_data_->GetPointData()->SetActiveVectors(attr_name.c_str());
            spdlog::info("vertex vector attribute '{}' found with {} components.", attr_name, array->GetNumberOfComponents());
            createGlyph3D(this->vertex_data_, { 1.0, 0.0, 0.0 }); // 红色
            break;
        }
    case FACE: {
        vtkDataArray* array = this->face_data_->GetCellData()->GetArray(attr_name.c_str());
        spdlog::info("face vector attribute '{}' found with {} components.", attr_name, array->GetNumberOfComponents());
        // 计算面中心点位置 =====
        vtkNew<vtkCellCenters> centers;
        centers->SetInputData(this->face_data_);
        centers->Update();
        // 将面中心点位置与向量数据合并 =====
        vtkNew<vtkPolyData> glyphInput;
        glyphInput->SetPoints(centers->GetOutput()->GetPoints());
        glyphInput->GetPointData()->SetVectors(array);
        createGlyph3D(glyphInput, { 0.0, 0.0, 1.0 }); // 蓝色

        break;
    }
    case EDGE: {

        break;
    }
    }
}

void MeshActor::setActiveRGBAttribute(std::string attr_name, ElementType type)
{
    switch (type) {
    case VERTEX: {
        vtkDataArray* array = this->vertex_data_->GetPointData()->GetArray(attr_name.c_str());
        this->vertex_data_->GetPointData()->SetActiveAttribute(attr_name.c_str(), vtkDataSetAttributes::SCALARS);
        vertex_mapper_->SetScalarVisibility(1);
        vertex_mapper_->SetColorModeToDirectScalars(); // 使用 RGB

        this->face_data_->GetPointData()->SetActiveScalars(attr_name.c_str());
        face_mapper_->SetScalarModeToUsePointData(); // 面使用点数据的插值
        face_mapper_->SetScalarVisibility(1);
        face_mapper_->SetColorModeToDirectScalars();

    } break;
    case FACE: {
        vtkDataArray* array = this->face_data_->GetCellData()->GetArray(attr_name.c_str());
        face_mapper_->SetScalarModeToUseCellData();
        this->face_data_->GetCellData()->SetActiveScalars(attr_name.c_str());
        face_mapper_->SetScalarVisibility(1);
        face_mapper_->SetColorModeToDirectScalars();

    } break;
    case EDGE:
        break;
    }
}

void MeshActor::setTextureImage(std::string attr_name, std::string texturePath)
{
    spdlog::info("start setTextureImage---------------------------");

    // 读取纹理贴图文件
    vtkNew<vtkImageReader2Factory> readerFactory;
    vtkSmartPointer<vtkImageReader2> textureFile = readerFactory->CreateImageReader2(texturePath.c_str());
    if (!textureFile) {
        spdlog::error("Error: Failed to create texture reader for {}", texturePath);
        return;
    }
    textureFile->SetFileName(texturePath.c_str());
    textureFile->Update();

    // 创建纹理对象
    vtkNew<vtkTexture> texture;
    texture->SetInputConnection(textureFile->GetOutputPort());
    texture->InterpolateOn(); // 启用插值使纹理更平滑

    // 读取传入的属性名作为UV
    vtkDataArray* tcoords = this->vertex_data_->GetPointData()->GetArray(attr_name.c_str());
    if (tcoords && tcoords->GetNumberOfComponents() == 2) {
        spdlog::info("use attribute [{}] as UV", attr_name);
        this->vertex_data_->GetPointData()->SetTCoords(tcoords);
        this->face_data_->GetPointData()->SetTCoords(tcoords);
    }
    this->face_actor_->SetTexture(texture);
}
void MeshActor::cancelTextureImage()
{
    this->face_actor_->SetTexture(nullptr);
}

void MeshActor::setAttriMode(
    const std::string& attr_name,
    Mode mode,
    ElementType type,
    const std::string& texturePath,
    double glyphScale,
    std::optional<std::pair<double, double>> scalarRange,
    bool resetScalarRange)
{
    cancelActiveAttribute();
    spdlog::info("Mode:{} type:{}", static_cast<int>(mode), static_cast<int>(type));
    switch (mode) {
    case SCALAR:
        setActiveScalarAttribute(attr_name, type);
        if (scalarRange.has_value()) {
            this->setScalarRange(scalarRange.value().first, scalarRange.value().second);
        }
        if (resetScalarRange)
            this->resetScalarRange();
        break;
    case VECTOR:
        setActiveVectorAttribute(attr_name, type);
        if (glyphScale > 0)
            this->setGlyph3DScaleFactor(glyphScale);
        break;
    case RGB:
        setActiveRGBAttribute(attr_name, type);
        break;
    case UV:
        setTextureImage(attr_name, texturePath);
        break;
    default:
        std::cout << "not the defalt mode" << std::endl;
        break;
    }
}
void MeshActor::cancelActiveAttribute()
{
    if (face_actor_->GetTexture() != nullptr) {
        cancelTextureImage();
    }
    glyph3D_actor_->SetVisibility(0);
    vertex_mapper_->SetScalarVisibility(0);
    edge_mapper_->SetScalarVisibility(0);
    face_mapper_->SetScalarVisibility(0);
    solid_mapper_->SetScalarVisibility(0);
}

// Glyph3D 的缩放因子调整接口
void MeshActor::setGlyph3DScaleFactor(double scale)
{

    vtkMapper* mapper = this->glyph3D_mapper_;
    if (!mapper)
        return;
    vtkAlgorithm* producer = nullptr;
    producer = mapper->GetInputConnection(0, 0)->GetProducer();
    if (producer) {
        vtkGlyph3D* glyph = vtkGlyph3D::SafeDownCast(producer);
        glyph->SetScaleFactor(scale);
        glyph->Update();
    }
}
// 标量的range映射标调整接口
void MeshActor::setScalarRange(double min, double max)
{
    vtkDataArray* array = nullptr;
    vtkPolyDataMapper* mapper = nullptr;

    // 如果点映射可见且有标量，设置点，同时同步设置面（点对面插值）
    if (vertex_mapper_->GetScalarVisibility() && vertex_data_ && vertex_data_->GetPointData()->GetScalars()) {
        array = vertex_data_->GetPointData()->GetScalars();
        mapper = vertex_mapper_;
        mapper->SetScalarRange(min, max);

        // 同步设置面（点对面插值时，面mapper用点数据）
        if (face_mapper_ && face_data_ && face_data_->GetPointData()->GetScalars()) {
            face_mapper_->SetScalarModeToUsePointData();
            face_mapper_->SetScalarRange(min, max);
        }
    } else // 否则面映射可见且有标量，设置面
        if (face_mapper_->GetScalarVisibility() && face_data_ && face_data_->GetCellData()->GetScalars()) {
            array = face_data_->GetCellData()->GetScalars();
            mapper = face_mapper_;
            mapper->SetScalarRange(min, max);
        }
    return;
}
// 重置标量映射范围到数据的实际范围
void MeshActor::resetScalarRange()
{
    // 优先判断面映射（cell data），否则判断点映射（point data）
    vtkDataArray* array = nullptr;
    vtkPolyDataMapper* mapper = nullptr;

    // 如果点映射可见且有标量，设置点，同时同步设置面（点对面插值）
    if (vertex_mapper_->GetScalarVisibility() && vertex_data_ && vertex_data_->GetPointData()->GetScalars()) {
        array = vertex_data_->GetPointData()->GetScalars();
        mapper = vertex_mapper_;
        double range[2];
        array->GetRange(range);
        mapper->SetScalarRange(range[0], range[1]);

        // 同步设置面（点对面插值时，面mapper用点数据）
        if (face_mapper_ && face_data_ && face_data_->GetPointData()->GetScalars()) {
            face_mapper_->SetScalarModeToUsePointData();
            face_mapper_->SetScalarRange(range[0], range[1]);
        }
        return;
    } else if // 负责如果面映射可见且有标量，设置面
        (face_mapper_->GetScalarVisibility() && face_data_ && face_data_->GetCellData()->GetScalars()) {
        array = face_data_->GetCellData()->GetScalars();
        mapper = face_mapper_;
        double range[2];
        array->GetRange(range);
        mapper->SetScalarRange(range[0], range[1]);
        return;
    }
}
void MeshActor::createGlyph3D(vtkDataSet* input, const std::array<double, 3>& color, double scale)
{
    vtkNew<vtkArrowSource> arrow_source; // 箭头源
    arrow_source->SetTipResolution(16);
    arrow_source->SetTipLength(0.3);
    arrow_source->SetTipRadius(0.1);

    vtkNew<vtkGlyph3D> glyph3D; // 过滤器
    glyph3D->SetSourceConnection(arrow_source->GetOutputPort());
    glyph3D->SetInputData(input);
    glyph3D->SetScaleModeToScaleByVector();
    glyph3D->SetScaleFactor(scale);
    glyph3D->OrientOn();
    glyph3D->Update();

    vtkPolyDataMapper* mapper = this->glyph3D_mapper_;
    mapper->SetInputConnection(glyph3D->GetOutputPort());
    mapper->ScalarVisibilityOff();

    vtkActor* actor = this->glyph3D_actor_;
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(const_cast<double*>(color.data()));
    actor->SetVisibility(1);
}