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
#include <vtkPointData.h>
#include <vtkLookupTable.h>
#include <vtkGlyph3D.h>
#include <vtkArrowSource.h>
#include <vtkAlgorithmOutput.h>
#include <vtkCellCenters.h>
#include <vtkCellDataToPointData.h>
#include <vtkImageReader2Factory.h>
#include <vtkImageReader2.h> 
#include <vtkTexture.h>
#include <vtkTextureMapToPlane.h>
#include <vtkPointDataToCellData.h>


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

        // 新增：动态处理顶点属性
        const size_t num_vertex = size;
        for (const auto& attr : model_data.vertex_attributes_) {
            const std::string& attr_name = attr.first;
            const std::vector<double>& attr_values = attr.second;
            bool isTriple = !attr_name.empty() && attr_name.back() == '3';
            bool isDouble = !attr_name.empty() && attr_name.back() == '2';
            if (!isTriple && !isDouble) {
                // 单分量属性 (如 "pressure")
                vtkNew<vtkDoubleArray> scalar_array;
                scalar_array->SetNumberOfComponents(1);
                scalar_array->SetName(attr_name.c_str());

                for (size_t i = 0; i < num_vertex; ++i) {
                    scalar_array->InsertNextValue(attr_values[i]);
                }
                vertex_poly->GetPointData()->AddArray(scalar_array);
            } else if (isDouble) {
                // 双元组属性 (如 "vertex_uv_2")
                vtkNew<vtkDoubleArray> vector_array;
                vector_array->SetNumberOfComponents(2);
                bool isUV = (attr_name.find("uv") != std::string::npos || attr_name.find("UV") != std::string::npos);
                if (isUV) {
                    std::cout << "Setting texture UV coordinates for attribute: " << attr_name << std::endl;
                    for (size_t i = 0; i < num_vertex; ++i) {
                        vector_array->InsertNextTuple2(
                            attr_values[i * 2],
                            attr_values[i * 2 + 1]);
                    }
                    vertex_poly->GetPointData()->SetTCoords(vector_array);
                    this->face_data_->GetPointData()->SetTCoords(vector_array);
                } else {
                    vector_array->SetName(attr_name.c_str());
                    for (size_t i = 0; i < num_vertex; ++i) {
                        vector_array->InsertNextTuple2(
                            attr_values[i * 2],
                            attr_values[i * 2 + 1]);
                    }
                    vertex_poly->GetPointData()->AddArray(vector_array);
                }
               vtkDataArray* tcoords = this->vertex_data_->GetPointData()->GetTCoords();
               if (tcoords) {
                   // 打印UV范围，正常应该在 [0,1] 之间
                   double minUV[2] = {1e10, 1e10}, maxUV[2] = {-1e10, -1e10};
                   for (int i = 0; i < tcoords->GetNumberOfTuples(); i++) {
                       double uv[2];
                       tcoords->GetTuple(i, uv);
                       minUV[0] = std::min(minUV[0], uv[0]);
                       minUV[1] = std::min(minUV[1], uv[1]);
                       maxUV[0] = std::max(maxUV[0], uv[0]);
                       maxUV[1] = std::max(maxUV[1], uv[1]);
                   }
                   std::cout << "UVscope: [" << minUV[0] << ", " << maxUV[0] << "] x [" 
                             << minUV[1] << ", " << maxUV[1] << "]" << std::endl;
               }
            }else if (isTriple) {
                // 三元组属性 (如 "vertex_vector_3")
                bool isColor = (attr_name.find("colors") != std::string::npos || attr_name.find("Colors") != std::string::npos
                    || attr_name.find("color") != std::string::npos || attr_name.find("Color") != std::string::npos);
                if (isColor) {
                    // 如果是颜色属性
                    vtkSmartPointer<vtkUnsignedCharArray> colors = vtkSmartPointer<vtkUnsignedCharArray>::New();
                    colors->SetNumberOfComponents(3);
                    colors->SetName(attr_name.c_str());

                    for (size_t i = 0; i < num_vertex; ++i) {
                        unsigned char r = static_cast<unsigned char>(attr_values[i * 3] * 255);
                        unsigned char g = static_cast<unsigned char>(attr_values[i * 3 + 1] * 255);
                        unsigned char b = static_cast<unsigned char>(attr_values[i * 3 + 2] * 255);
                        colors->InsertNextTuple3(r, g, b);
                    }
                    vertex_poly->GetPointData()->AddArray(colors);
                }
                    else// 否则作为向量处理
                    {
                        vtkNew<vtkDoubleArray> vector_array;
                        vector_array->SetNumberOfComponents(3);
                        vector_array->SetName(attr_name.c_str());

                        for (size_t i = 0; i < num_vertex; ++i) {
                            vector_array->InsertNextTuple3(
                                attr_values[i * 3],
                                attr_values[i * 3 + 1],
                                attr_values[i * 3 + 2]);
                        }
                        vertex_poly->GetPointData()->AddArray(vector_array);
                    
                    }
                    //// 输出vector_array看是否正确
                    //vtkDoubleArray* test = static_cast<vtkDoubleArray*>(vertex_poly->GetPointData()->GetArray(attr_name.c_str()));
                    //for (vtkIdType i = 0; i < test->GetNumberOfTuples(); i++) {
                    //    double testValue[3];
                    //    test->GetTuple(i, testValue);
                    //    std::cout << "vertex" << ":" << attr_name.c_str() << ":" << i << ":" << testValue[0] << "," << testValue[1] << "," << testValue[2] << std::endl;
                    //}
                }
            
            else {
                // 错误处理：属性长度不匹配
                std::cerr << "Warning: Vertex attribute '" << attr_name
                          << "' has invalid length (" << attr_values.size()
                          << "), expected " << num_vertex << " or " << 3 * num_vertex << std::endl;
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
                vtkNew<vtkDoubleArray> scalar_array;
                scalar_array->SetNumberOfComponents(1);
                scalar_array->SetName(attr_name.c_str());

                for (size_t i = 0; i < num_faces; ++i) {
                    scalar_array->InsertNextValue(attr_values[i]);
                }
                face_poly->GetCellData()->AddArray(scalar_array);
            } else if (isTriple) {
                // 判断是否是颜色属性
                bool isColor = (attr_name.find("colors") != std::string::npos || attr_name.find("Colors") != std::string::npos
                    || attr_name.find("color") != std::string::npos || attr_name.find("Color") != std::string::npos);
                if (isColor) {
                    // 如果是颜色属性
                    vtkSmartPointer<vtkUnsignedCharArray> colors = vtkSmartPointer<vtkUnsignedCharArray>::New();
                    colors->SetNumberOfComponents(3);
                    colors->SetName(attr_name.c_str());

                    for (size_t i = 0; i < num_faces; ++i) {
                        unsigned char r = static_cast<unsigned char>(attr_values[i * 3] * 255);
                        unsigned char g = static_cast<unsigned char>(attr_values[i * 3 + 1] * 255);
                        unsigned char b = static_cast<unsigned char>(attr_values[i * 3 + 2] * 255);
                        colors->InsertNextTuple3(r, g, b);
                    }
                    face_poly->GetCellData()->AddArray(colors);
                } else {
                    // 否则作为向量处理
                    vtkNew<vtkDoubleArray> vector_array;
                    vector_array->SetNumberOfComponents(3);
                    vector_array->SetName(attr_name.c_str());

                    for (size_t i = 0; i < num_faces; ++i) {
                        vector_array->InsertNextTuple3(
                            attr_values[i * 3],
                            attr_values[i * 3 + 1],
                            attr_values[i * 3 + 2]);
                    }
                    face_poly->GetCellData()->AddArray(vector_array);
                }
                //// 输出face的vector_array看是否正确
                //vtkDoubleArray* test = static_cast<vtkDoubleArray*>(face_poly->GetCellData()->GetArray(attr_name.c_str()));
                //for (vtkIdType i = 0; i < test->GetNumberOfTuples(); i++) {
                //    double testValue[3];
                //    test->GetTuple(i, testValue);
                //    std::cout << "face" << i << ":" << attr_name.c_str() << ":" << testValue[0] << "," << testValue[1] << "," << testValue[2] << std::endl;
                //}
            } else {
                // 错误处理：属性长度不匹配
                std::cerr << "Warning: Face attribute '" << attr_name
                          << "' has invalid length (" << attr_values.size()
                          << "), expected " << num_faces << " or " << 3 * num_faces << std::endl;
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
        // 新增：动态处理边属性
        const size_t num_edges = edge_poly->GetNumberOfCells();
        for (const auto& attr : model_data.edge_attributes_) {
            const std::string& attr_name = attr.first;
            const std::vector<double>& attr_values = attr.second;
            if (attr_values.size() == num_edges) {
                // 单分量属性 (如 "edge_weight")
                vtkNew<vtkDoubleArray> scalar_array;
                scalar_array->SetNumberOfComponents(1);
                scalar_array->SetName(attr_name.c_str());
                for (size_t i = 0; i < num_edges; ++i) {
                    scalar_array->InsertNextValue(attr_values[i]);
                }
                edge_poly->GetCellData()->AddArray(scalar_array);
            } else if (attr_values.size() == 3 * num_edges) {
                // 三元组属性 (如 "edge_direction_3")
                vtkNew<vtkDoubleArray> vector_array;
                vector_array->SetNumberOfComponents(3);
                vector_array->SetName(attr_name.c_str());
                for (size_t i = 0; i < num_edges; ++i) {
                    vector_array->InsertNextTuple3(
                        attr_values[i * 3],
                        attr_values[i * 3 + 1],
                        attr_values[i * 3 + 2]);
                }
                edge_poly->GetCellData()->AddArray(vector_array);
            } else {
                // 错误处理：属性长度不匹配
                std::cerr << "Warning: Edge attribute '" << attr_name
                          << "' has invalid length (" << attr_values.size()
                          << "), expected " << num_edges << " or " << 3 * num_edges << std::endl;
            }
        }
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

void MeshActor::setActiveScalarAttribute(std::string attr_name, ElementType type)
{
    switch (type) {
    case VERTEX:
        if (this->vertex_data_) {
            vtkDataArray* array = this->vertex_data_->GetPointData()->GetArray(attr_name.c_str());
            this->vertex_data_->GetPointData()->SetActiveAttribute(attr_name.c_str(), vtkDataSetAttributes::SCALARS);
            // 设置映射范围
            double range[2];
            array->GetRange(range);
            vertex_mapper_->SetScalarRange(range[0], range[1]); 
            vertex_mapper_->SetScalarVisibility(1);
            // 如果是 RGB 颜色数组（组件数=3 且类型是 unsigned char）
            if (array->GetNumberOfComponents() == 3 && array->GetDataType() == VTK_UNSIGNED_CHAR) {

                vertex_mapper_->SetColorModeToDirectScalars(); // 使用 RGB
                if (this->face_data_) {
                    this->face_data_->GetPointData()->SetActiveScalars(attr_name.c_str());
                    face_mapper_->SetScalarModeToUsePointData();
                    face_mapper_->SetScalarVisibility(1);
                    face_mapper_->SetScalarRange(range[0], range[1]);
                    face_mapper_->SetColorModeToDirectScalars(); // 使用 RGB

                }
            } else {
                vertex_mapper_->SetColorModeToMapScalars(); // 使用 colormap
                if (this->face_data_) {
                    this->face_data_->GetPointData()->SetActiveScalars(attr_name.c_str());
                    face_mapper_->SetScalarModeToUsePointData();
                    face_mapper_->SetScalarVisibility(1);
                    face_mapper_->SetScalarRange(range[0], range[1]);
                    face_mapper_->SetColorModeToMapScalars();
                }
            }
            vertex_mapper_->Update();
            face_mapper_->Update();
            renderer_->Render();
        }
        break;
    case FACE:
        if (this->face_data_) {
            vtkDataArray* array = this->face_data_->GetCellData()->GetArray(attr_name.c_str());
            // 设置映射范围
            double range[2];
            array->GetRange(range); 
            face_mapper_->SetScalarRange(range[0], range[1]);

            face_mapper_->SetScalarModeToUseCellData(); 
            this->face_data_->GetCellData()->SetActiveScalars(attr_name.c_str());
            face_mapper_->SetScalarVisibility(1);
            // 如果是 RGB 颜色数组（组件数=3 且类型是 unsigned char）
            if (array->GetNumberOfComponents() == 3 && array->GetDataType() == VTK_UNSIGNED_CHAR) {

                face_mapper_->SetColorModeToDirectScalars(); // 直接使用 RGB
            } else {
                face_mapper_->SetColorModeToMapScalars(); // 使用 colormap
            }
            face_mapper_->Update();
            renderer_->Render();
        }
        break;
    case EDGE:
        if (this->edge_data_) {
            this->edge_data_->GetCellData()->SetActiveAttribute(attr_name.c_str(), vtkDataSetAttributes::SCALARS);
            edge_mapper_->SetScalarModeToUseCellData();
            edge_mapper_->SetScalarVisibility(1);
            edge_mapper_->Update();
            renderer_->Render();
        }
        break;
    }
}

void MeshActor::setActiveVectorAttribute(std::string attr_name, ElementType type)
{
    switch (type) {
    case VERTEX:
        if (this->vertex_data_) { 
            vtkDataArray* array = this->vertex_data_->GetPointData()->GetArray(attr_name.c_str());
            if (array) {
                this->vertex_data_->GetPointData()->SetActiveVectors(attr_name.c_str());
                vtkSmartPointer<vtkActor> normalsActor;
                std::cout<< "vertex vector attribute '" << attr_name << "' found with "
                          << array->GetNumberOfComponents() << " components." << std::endl;
                // 创建箭头源并配置 创建法向量可视化（使用箭头glyph）
                vtkNew<vtkArrowSource> arrowSource;
                arrowSource->SetTipResolution(16); // 设置箭头尖端的分辨率（侧面数）
                arrowSource->SetTipLength(0.3);
                arrowSource->SetTipRadius(0.1);
                // 创建Glyph过滤器并配置
                vtkNew<vtkGlyph3D> glyph3D;
                glyph3D->SetSourceConnection(arrowSource->GetOutputPort()); // 设置glyph源为箭头
                glyph3D->SetInputData(this->vertex_data_); // 设置输入数据
                //glyph3D->SetVectorModeToUseVector();                       // 使用x向量模式定向glyph
                glyph3D->SetScaleModeToScaleByVector(); // 使用向量模式缩放glyph
                glyph3D->SetScaleFactor(0.3); // 设置缩放因子
                glyph3D->OrientOn(); // 开启方向功能
                glyph3D->Update(); // 执行glyph生成

                vtkNew<vtkPolyDataMapper> normalsMapper;
                normalsMapper->SetInputConnection(glyph3D->GetOutputPort());
                normalsMapper->ScalarVisibilityOff();

                normalsActor = vtkSmartPointer<vtkActor>::New();
                normalsActor->SetMapper(normalsMapper);
                normalsActor->GetProperty()->SetColor(colors->GetColor3d("Red").GetData()); // 法向量箭头为红色
                // 在添加新Actor前移除旧的
                vtkActorCollection* actorCollection = renderer_->GetActors();
                actorCollection->InitTraversal();
                for (vtkIdType i = 0; i < actorCollection->GetNumberOfItems(); ++i) {
                    vtkActor* actor = vtkActor::SafeDownCast(actorCollection->GetNextActor());
                        vtkAlgorithm* producer = nullptr;
                        if (actor->GetMapper() && actor->GetMapper()->GetInputConnection(0, 0)) {
                            producer = actor->GetMapper()->GetInputConnection(0, 0)->GetProducer();
                        }
                        if (producer && std::string(producer->GetClassName()) == "vtkGlyph3D") {
                            std::cout << "--------------" << "remove old glyph3D" << "' -----" << std::endl;
                            renderer_->RemoveActor(actor);
                            break;
                        }
                }
                renderer_->AddActor(normalsActor);
            }
            vertex_mapper_->Update();
            renderer_->Render();
        }
        break;
    case FACE:
        if (this->face_data_) {
            vtkDataArray* array = this->face_data_->GetCellData()->GetArray(attr_name.c_str());
            if (array) {
                std::cout << "Face vector attribute '" << attr_name << "' found with "
                          << array->GetNumberOfComponents() << " components." << std::endl;

                // 计算面中心点位置 =====
                vtkNew<vtkCellCenters> centers;
                centers->SetInputData(this->face_data_);
                
                centers->Update();

                // 将面中心点位置与向量数据合并 =====
                vtkNew<vtkPolyData> glyphInput;
                glyphInput->SetPoints(centers->GetOutput()->GetPoints());
                glyphInput->GetPointData()->SetVectors(array);

                // 创建箭头源
                vtkNew<vtkArrowSource> arrowSource;
                arrowSource->SetTipResolution(16);
                arrowSource->SetTipLength(0.3);
                arrowSource->SetTipRadius(0.1);

                // 创建Glyph过滤器
                vtkNew<vtkGlyph3D> glyph3D;
                glyph3D->SetSourceConnection(arrowSource->GetOutputPort());
                glyph3D->SetInputData(glyphInput);
                //glyph3D->SetVectorModeToUseVector(); 
                glyph3D->SetScaleModeToScaleByVector(); // 按向量长度缩放
                glyph3D->SetScaleFactor(0.3); // 缩放因子
                glyph3D->Update();
                vtkDataArray* vectors = glyphInput->GetPointData()->GetVectors();
                if (vectors) {
                    double vec[3];
                    vectors->GetTuple(0, vec); // 获取第一个面的向量
                    std::cout << "First face vector: " << vec[0] << ", " << vec[1] << ", " << vec[2] << std::endl;
                }
                // 创建映射器和演员
                vtkNew<vtkPolyDataMapper> glyphMapper;
                glyphMapper->SetInputConnection(glyph3D->GetOutputPort());
                glyphMapper->ScalarVisibilityOff();

                vtkSmartPointer<vtkActor> glyphActor = vtkSmartPointer<vtkActor>::New();
                glyphActor->SetMapper(glyphMapper);
                glyphActor->GetProperty()->SetColor(colors->GetColor3d("Blue").GetData());

                // 清除旧的Glyph3D演员
                vtkActorCollection* actorCollection = renderer_->GetActors();
                actorCollection->InitTraversal();
                for (vtkIdType i = 0; i < actorCollection->GetNumberOfItems(); ++i) {
                    vtkActor* actor = vtkActor::SafeDownCast(actorCollection->GetNextActor());
                    vtkAlgorithm* producer = nullptr;
                    if (actor->GetMapper() && actor->GetMapper()->GetInputConnection(0, 0)) {
                        producer = actor->GetMapper()->GetInputConnection(0, 0)->GetProducer();
                    }
                    if (producer && std::string(producer->GetClassName()) == "vtkGlyph3D") {
                        renderer_->RemoveActor(actor);
                        break;
                    }
                }

                renderer_->AddActor(glyphActor);
                renderer_->Render();
            }
        }
        break;
    case EDGE:
        if (this->edge_data_) { 
            this->edge_data_->GetCellData()->SetActiveAttribute(attr_name.c_str(), vtkDataSetAttributes::VECTORS);
            edge_mapper_->SetScalarModeToUseCellData();
            //edge_mapper_->SetVectorModeToUseVector();
            edge_mapper_->Update();
            renderer_->Render();
        }
        break;
    }
}

void MeshActor::setTextureImage(std::string texturePath)
{
    std::cout << "start setTextureImage---------------------------" << std::endl;

    //  读取纹理贴图文件
    vtkNew<vtkImageReader2Factory> readerFactory;
    vtkSmartPointer<vtkImageReader2> textureFile;
    textureFile.TakeReference(readerFactory->CreateImageReader2(texturePath.c_str()));

    if (!textureFile) {
        std::cerr << "Error: Failed to create texture reader for " << texturePath << std::endl;
        return;
    }
    textureFile->SetFileName(texturePath.c_str());
    textureFile->Update();
    // 创建纹理对象
    vtkNew<vtkTexture> texture;
    texture->SetInputConnection(textureFile->GetOutputPort());
    texture->InterpolateOn(); // 启用插值使纹理更平滑

    // 如果模型没有 UV，这里会自动触发计算
    if ((this->vertex_data_->GetPointData()->GetTCoords()) ){
        std::cout << "重新计算uv" << std::endl;
        vtkNew<vtkTextureMapToPlane> textureMapper;
        textureMapper->SetInputData(this->vertex_data_);
        textureMapper->Update();

        // 将计算的 UV 设置到模型
        this->vertex_data_->GetPointData()->SetTCoords(
            textureMapper->GetOutput()->GetPointData()->GetTCoords());
   
    }

    this->face_actor_->SetTexture(texture);
    renderer_->Render();
}
void MeshActor::cancelTextureImage()
{
    this->face_actor_->SetTexture(nullptr);
    std::cout << "取消texture" << std::endl;
    renderer_->Render();
}
void MeshActor::setAttriMode(std::string attr_name, Mode mode, ElementType type, std::string texturePath)
{
    cancelActiveAttribute();
    std::cout << "Mode:"<<mode << "type:"<<type << std::endl;
    switch (mode) {
    case SCALAR:
        setActiveScalarAttribute(attr_name, type); 
        break;
    case VECTOR:
        setActiveVectorAttribute(attr_name, type); 
        break;
    case RGB:
        // RGB通常是颜色属性，直接设置为标量可视化并开启直接颜色模式
        setActiveScalarAttribute(attr_name, type); 
        break;
    case UV:
        // UV坐标用于纹理映射
        setTextureImage(texturePath);
        break;
    default:
        std::cout << "not the defalt mode" << std::endl;
        break;
    }
}
void MeshActor::cancelActiveAttribute()
{
        cancelTextureImage();
        cancelActiveGlyph3D();
        vertex_mapper_->SetScalarVisibility(0);
        edge_mapper_->SetScalarVisibility(0);
        face_mapper_->SetScalarVisibility(0);
        solid_mapper_->SetScalarVisibility(0);
}
void MeshActor::cancelActiveGlyph3D()
{
    // 移除vtkGlyph3D的actor
    vtkActorCollection* actorCollection = renderer_->GetActors();
    actorCollection->InitTraversal();
    for (vtkIdType i = 0; i < actorCollection->GetNumberOfItems(); ++i) {
        vtkActor* actor = vtkActor::SafeDownCast(actorCollection->GetNextActor());
        vtkAlgorithm* producer = nullptr;
        if (actor->GetMapper() && actor->GetMapper()->GetInputConnection(0, 0)) {
            producer = actor->GetMapper()->GetInputConnection(0, 0)->GetProducer();
        }
        if (producer && std::string(producer->GetClassName()) == "vtkGlyph3D") {
            renderer_->RemoveActor(actor);
            break;
        }
    }
}

// Glyph3D 的缩放因子调整接口
void MeshActor::setGlyph3DScaleFactor(double scale)
{
    // 遍历renderer中的actor，找到Glyph3D生成的actor并调整其scale factor
    vtkActorCollection* actorCollection = renderer_->GetActors();
    actorCollection->InitTraversal();
    for (vtkIdType i = 0; i < actorCollection->GetNumberOfItems(); ++i) {
        vtkActor* actor = vtkActor::SafeDownCast(actorCollection->GetNextActor());
        if (!actor)
            continue;
        vtkMapper* mapper = actor->GetMapper();
        if (mapper && mapper->GetInputConnection(0, 0)) {
            vtkAlgorithm* producer = mapper->GetInputConnection(0, 0)->GetProducer();
            if (producer && std::string(producer->GetClassName()) == "vtkGlyph3D") {
                vtkGlyph3D* glyph = vtkGlyph3D::SafeDownCast(producer);
                if (glyph) {
                    glyph->SetScaleFactor(scale);
                    glyph->Update();
                    renderer_->Render();
                }
            }
        }
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
        mapper->Update();

        // 同步设置面（点对面插值时，面mapper用点数据）
        if (face_mapper_ && face_data_ && face_data_->GetPointData()->GetScalars()) {
            face_mapper_->SetScalarModeToUsePointData();
            face_mapper_->SetScalarRange(min, max);
            face_mapper_->Update();
        }
    }else     // fz面映射可见且有标量，设置面
        if (face_mapper_->GetScalarVisibility() && face_data_ && face_data_->GetCellData()->GetScalars()) {
            array = face_data_->GetCellData()->GetScalars();
            mapper = face_mapper_;
            mapper->SetScalarRange(min, max);
            mapper->Update();
        }
    renderer_->Render();
    return;
}

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
        mapper->Update();

        // 同步设置面（点对面插值时，面mapper用点数据）
        if (face_mapper_ && face_data_ && face_data_->GetPointData()->GetScalars()) {
            face_mapper_->SetScalarModeToUsePointData();
            face_mapper_->SetScalarRange(range[0], range[1]);
            face_mapper_->Update();
        }
        renderer_->Render();
        return;
    } else if // 负责如果面映射可见且有标量，设置面
         (face_mapper_->GetScalarVisibility() && face_data_ && face_data_->GetCellData()->GetScalars()) {
            array = face_data_->GetCellData()->GetScalars();
            mapper = face_mapper_;
            double range[2];
            array->GetRange(range);
            mapper->SetScalarRange(range[0], range[1]);
            mapper->Update();
            renderer_->Render();
            return;
        }
}
// todo
// 接口整理  
// 提交优化
// vtk插件分别提交
// cancel 纹理贴图接口