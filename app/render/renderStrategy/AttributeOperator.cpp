#include "AttributeOperator.h"
#include <vtkCellCenters.h>
#include <vtkPolyData.h>
#include <vtkCellData.h>
#include <vtkDataSet.h>
#include <vtkDoubleArray.h>
#include <vtkMapper.h>
#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>
#include <cassert>
#include <vtkPointData.h> 
#include <vtkPoints.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
constexpr Index kMaxEdgeSamples = 1000;

//! @brief 首次请求时计算单元中心，后续向量渲染直接复用缓存
vtkPolyData* ensureCellCenters(vtkDataSet& data, vtkSmartPointer<vtkPolyData>& cache)
{
    if (!cache) {
        vtkNew<vtkCellCenters> centers;
        centers->SetInputData(&data);
        centers->Update();

        // vtkSmartPointer 持有输出对象，局部 filter 析构后缓存仍然有效。
        cache = centers->GetOutput();
    }
    return cache.GetPointer();
}

double distanceBetweenPoints(vtkPoints& points, Index point_id_a, Index point_id_b)
{
    double a[3] {};
    double b[3] {};
    points.GetPoint(point_id_a, a);
    points.GetPoint(point_id_b, b);

    const double dx = a[0] - b[0];
    const double dy = a[1] - b[1];
    const double dz = a[2] - b[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// 计算用于 glyph 默认尺寸的典型边长；边很多时用固定上限做均匀采样。
double averageEdgeLength(vtkPoints& points, const std::vector<Index>& edge_cells)
{
    const Index edge_count = static_cast<Index>(edge_cells.size() / 2);
    if (edge_count <= 0)
        return 0.0;

    const Index sample_count = std::min(edge_count, kMaxEdgeSamples);

    double total_length = 0.0;
    Index valid_count = 0;
    for (Index sample_index = 0; sample_index < sample_count && valid_count < kMaxEdgeSamples; ++sample_index) {
        const Index edge_index = sample_index * edge_count / sample_count;
        const Index point_id_a = edge_cells[edge_index * 2];
        const Index point_id_b = edge_cells[edge_index * 2 + 1];
        if (point_id_a < 0 || point_id_b < 0)
            continue;
        if (point_id_a >= points.GetNumberOfPoints() || point_id_b >= points.GetNumberOfPoints())
            continue;

        const double length = distanceBetweenPoints(points, point_id_a, point_id_b);
        if (length <= 0.0)
            continue;

        total_length += length;
        ++valid_count;
    }

    return valid_count > 0 ? total_length / valid_count : 0.0;
}

// 没有显式边数据时，从面片相邻顶点中均匀采样，估算典型边长。
double averageFaceEdgeLength(vtkPoints& points, const std::vector<Index>& face_cells, const std::vector<Index>& face_offsets)
{
    const Index face_count = static_cast<Index>(face_offsets.size()) - 1;
    if (face_count <= 0 || face_cells.empty())
        return 0.0;

    const Index total_face_edges = static_cast<Index>(face_cells.size());
    const Index sample_count = std::min(total_face_edges, kMaxEdgeSamples);
    const Index sample_step = std::max<Index>(1, total_face_edges / sample_count);

    double total_length = 0.0;
    Index valid_count = 0;
    Index visited_edge_count = 0;

    // 按固定步长跳采面边；这里只估算典型边长，不需要精确遍历全部边。
    for (Index face_index = 0; face_index < face_count && valid_count < sample_count; ++face_index) {
        const Index begin = face_offsets[face_index];
        const Index end = face_offsets[face_index + 1];
        const Index point_count = end - begin;
        if (point_count < 2)
            continue;
        if (begin < 0 || end > total_face_edges)
            continue;

        for (Index local_index = 0; local_index < point_count && valid_count < sample_count; ++local_index) {
            if (visited_edge_count % sample_step != 0) {
                ++visited_edge_count;
                continue;
            }

            const Index current_edge_index = begin + local_index;
            const Index point_id_a = face_cells[current_edge_index];
            const Index point_id_b = face_cells[begin + ((local_index + 1) % point_count)];
            if (point_id_a < 0 || point_id_b < 0) {
                ++visited_edge_count;
                continue;
            }
            if (point_id_a >= points.GetNumberOfPoints() || point_id_b >= points.GetNumberOfPoints()) {
                ++visited_edge_count;
                continue;
            }

            const double length = distanceBetweenPoints(points, point_id_a, point_id_b);
            if (length > 0.0) {
                total_length += length;
                ++valid_count;
            }
            ++visited_edge_count;
        }
    }

    return valid_count > 0 ? total_length / valid_count : 0.0;
}

}

AttributeOperator::AttributeOperator(MeshActor* mesh_actor)
    : mesh_actor_(mesh_actor) {};

vtkPolyDataMapper* AttributeOperator::getFaceMapper() { 
    return mesh_actor_->face_mapper_; 
}

vtkPolyDataMapper* AttributeOperator::getEdgeMapper()
{
    return mesh_actor_->edge_mapper_;
}

vtkPolyDataMapper* AttributeOperator::getSolidMapper()
{
    return mesh_actor_->solid_mapper_;
}

vtkPolyDataMapper* AttributeOperator::getGlyph3DMapper() { 
    return mesh_actor_->glyph3D_mapper_; 
}

vtkActor* AttributeOperator::getGlyph3DActor() { 
    return mesh_actor_->glyph3D_actor_; 
}

vtkActor* AttributeOperator::getFaceActor()
{
    return mesh_actor_->face_actor_;
}

vtkCellData* AttributeOperator::getFaceCellData()
{ 
    return mesh_actor_->face_data_->GetCellData();
}

vtkPointData* AttributeOperator::getFacePointData()
{
    return mesh_actor_->face_data_->GetPointData();
}

vtkCellData* AttributeOperator::getEdgeCellData()
{
    return mesh_actor_->edge_data_->GetCellData();
}

vtkCellData* AttributeOperator::getSolidCellData()
{
    return mesh_actor_->solid_data_->GetCellData();
}

vtkPointData* AttributeOperator::getSolidPointData()
{
    return mesh_actor_->solid_data_->GetPointData();
}

void AttributeOperator::enableFaceAttributeOffset()
{
    auto& offset = mesh_actor_->face_attribute_offset_;
    if (!offset.active) {
        mesh_actor_->face_mapper_->GetRelativeCoincidentTopologyPolygonOffsetParameters(
            offset.factor,
            offset.units);
        offset.active = true;
    }

    // 只修改当前 face mapper 的相对偏移，避免面属性和体外表面共面时互相遮挡。
    mesh_actor_->face_mapper_->SetRelativeCoincidentTopologyPolygonOffsetParameters(-1.0, -1.0);
}

void AttributeOperator::disableFaceAttributeOffset()
{
    auto& offset = mesh_actor_->face_attribute_offset_;
    if (!offset.active)
        return;

    mesh_actor_->face_mapper_->SetRelativeCoincidentTopologyPolygonOffsetParameters(
        offset.factor,
        offset.units);
    offset.active = false;
}

double AttributeOperator::getMeshScale() const noexcept
{
    vtkPoints* points = mesh_actor_->points_;
    const auto& model_data = mesh_actor_->model_data_;
    if (!points || !model_data)
        return 1.0;

    const double edge_scale = averageEdgeLength(*points, model_data->vtk_edge_cells_);
    if (edge_scale > 0.0)
        return edge_scale;

    const double face_edge_scale = averageFaceEdgeLength(
        *points, model_data->vtk_face_cells_, model_data->vtk_face_cells_offset_);
    if (face_edge_scale > 0.0)
        return face_edge_scale;

    return 1.0;
}

vtkSmartPointer<vtkPolyData> AttributeOperator::getFaceGlyphInput(const std::string& attr_name)
{
    vtkPolyData* face_data = mesh_actor_->face_data_;
    if (!face_data)
        return nullptr;

    vtkDataArray* array = face_data->GetCellData()->GetArray(attr_name.c_str());
    if (!array)
        return nullptr;

    vtkPolyData* centers = ensureCellCenters(*face_data, mesh_actor_->face_cell_centers_);
    if (!centers || !centers->GetPoints())
        return nullptr;

    // 合并缓存的单元中心点与向量数据，作为 glyph 输入。
    vtkSmartPointer<vtkPolyData> glyphInput = vtkSmartPointer<vtkPolyData>::New();
    glyphInput->SetPoints(centers->GetPoints());
    glyphInput->GetPointData()->SetVectors(array);

    return glyphInput;
}

vtkSmartPointer<vtkPolyData> AttributeOperator::getEdgeGlyphInput(const std::string& attr_name)
{
    vtkPolyData* edge_data = mesh_actor_->edge_data_;
    if (!edge_data)
        return nullptr;

    vtkDataArray* array = edge_data->GetCellData()->GetArray(attr_name.c_str());
    if (!array)
        return nullptr;

    // 边向量位于线段中心，首次请求时计算并缓存中心点。
    vtkPolyData* centers = ensureCellCenters(*edge_data, mesh_actor_->edge_cell_centers_);
    if (!centers || !centers->GetPoints())
        return nullptr;

    vtkSmartPointer<vtkPolyData> glyph_input = vtkSmartPointer<vtkPolyData>::New();
    glyph_input->SetPoints(centers->GetPoints());
    glyph_input->GetPointData()->SetVectors(array);
    return glyph_input;
}

vtkSmartPointer<vtkPolyData> AttributeOperator::getPointGlyphInput(const std::string& attr_name)
{
    vtkPolyData* face_data = mesh_actor_->face_data_;
    if (!face_data)
        return nullptr;
    vtkDataArray* array = face_data->GetPointData()->GetArray(attr_name.c_str());
    if (!array)
        return nullptr;

    vtkPoints* component_points = mesh_actor_->points_;
    const auto& model_data = mesh_actor_->model_data_;
    if (!component_points || !model_data)
        return nullptr;

    const int component_count = array->GetNumberOfComponents();
    auto vectors = vtkSmartPointer<vtkDoubleArray>::New();
    vectors->SetName(attr_name.c_str());
    vectors->SetNumberOfComponents(component_count);

    // 点属性数组与组件私有点集同为局部约定，tuple 与局部点 id 一一对应，直接顺序取。
    auto points = vtkSmartPointer<vtkPoints>::New();
    std::vector<double> tuple(static_cast<size_t>(component_count));
    const vtkIdType point_count = component_points->GetNumberOfPoints();
    for (vtkIdType local_point_id = 0; local_point_id < point_count; ++local_point_id) {
        if (local_point_id >= array->GetNumberOfTuples())
            break;

        double point[3] {};
        component_points->GetPoint(local_point_id, point);
        points->InsertNextPoint(point);

        array->GetTuple(local_point_id, tuple.data());
        vectors->InsertNextTuple(tuple.data());
    }

    auto glyphInput = vtkSmartPointer<vtkPolyData>::New();
    glyphInput->SetPoints(points);
    glyphInput->GetPointData()->SetVectors(vectors);
    return glyphInput;
}

vtkSmartPointer<vtkPolyData> AttributeOperator::getSolidGlyphInput(const std::string& attr_name)
{
    vtkUnstructuredGrid* solid_data = mesh_actor_->solid_data_;
    if (!solid_data)
        return nullptr;

    vtkDataArray* array = solid_data->GetCellData()->GetArray(attr_name.c_str());
    if (!array)
        return nullptr;

    vtkPolyData* centers = ensureCellCenters(*solid_data, mesh_actor_->solid_cell_centers_);
    if (!centers || !centers->GetPoints())
        return nullptr;

    vtkSmartPointer<vtkPolyData> glyphInput = vtkSmartPointer<vtkPolyData>::New();
    glyphInput->SetPoints(centers->GetPoints());
    glyphInput->GetPointData()->SetVectors(array);

    return glyphInput;
}
