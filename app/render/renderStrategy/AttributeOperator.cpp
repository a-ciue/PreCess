#include "AttributeOperator.h"
#include <vtkPolyData.h>
#include <vtkCellCenters.h>
#include <vtkCellData.h>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>
#include <cassert>
#include <vtkPointData.h> 
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
constexpr Index kMaxEdgeSamples = 1000;

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

    double total_length = 0.0;
    Index valid_count = 0;
    Index sample_index = 0;

    // 按面顺序遍历，但只在均匀采样命中的边上计算长度，最多统计固定上限条边。
    for (Index face_index = 0; face_index < face_count && sample_index < sample_count; ++face_index) {
        const Index begin = face_offsets[face_index];
        const Index end = face_offsets[face_index + 1];
        const Index point_count = end - begin;
        if (point_count < 2)
            continue;
        if (begin < 0 || end > total_face_edges)
            continue;

        for (Index local_index = 0; local_index < point_count && sample_index < sample_count; ++local_index) {
            const Index target_edge_index = sample_index * total_face_edges / sample_count;
            const Index current_edge_index = begin + local_index;
            if (current_edge_index < target_edge_index)
                continue;

            const Index point_id_a = face_cells[current_edge_index];
            const Index point_id_b = face_cells[begin + ((local_index + 1) % point_count)];
            if (point_id_a < 0 || point_id_b < 0) {
                ++sample_index;
                continue;
            }
            if (point_id_a >= points.GetNumberOfPoints() || point_id_b >= points.GetNumberOfPoints()) {
                ++sample_index;
                continue;
            }

            const double length = distanceBetweenPoints(points, point_id_a, point_id_b);
            if (length > 0.0) {
                total_length += length;
                ++valid_count;
            }
            ++sample_index;
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

vtkCellData* AttributeOperator::getSolidCellData()
{
    return mesh_actor_->solid_data_->GetCellData();
}

vtkPointData* AttributeOperator::getSolidPointData()
{
    return mesh_actor_->solid_data_->GetPointData();
}

double AttributeOperator::getMeshScale() const noexcept
{
    vtkPoints* points = mesh_actor_->global_points_;
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

    // 计算面中心点位置
    vtkNew<vtkCellCenters> centers;
    centers->SetInputData(face_data);
    centers->Update();

    // 合并面中心点与向量数据
    vtkSmartPointer<vtkPolyData> glyphInput = vtkSmartPointer<vtkPolyData>::New();
    glyphInput->SetPoints(centers->GetOutput()->GetPoints());
    glyphInput->GetPointData()->SetVectors(array);

    return glyphInput;
}

vtkSmartPointer<vtkPolyData> AttributeOperator::getPointGlyphInput(const std::string& attr_name)
{
    vtkPolyData* face_data = mesh_actor_->face_data_;
    if (!face_data)
        return nullptr;
    vtkDataArray* array = face_data->GetPointData()->GetArray(attr_name.c_str());
    if (!array)
        return nullptr;
    face_data->GetPointData()->SetActiveVectors(attr_name.c_str());
    return face_data;
}

vtkSmartPointer<vtkPolyData> AttributeOperator::getSolidGlyphInput(const std::string& attr_name)
{
    vtkUnstructuredGrid* solid_data = mesh_actor_->solid_data_;
    if (!solid_data)
        return nullptr;

    vtkDataArray* array = solid_data->GetCellData()->GetArray(attr_name.c_str());
    if (!array)
        return nullptr;

    vtkNew<vtkCellCenters> centers;
    centers->SetInputData(solid_data);
    centers->Update();

    vtkSmartPointer<vtkPolyData> glyphInput = vtkSmartPointer<vtkPolyData>::New();
    glyphInput->SetPoints(centers->GetOutput()->GetPoints());
    glyphInput->GetPointData()->SetVectors(array);

    return glyphInput;
}
