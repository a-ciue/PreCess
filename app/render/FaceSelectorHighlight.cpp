#include "CoincidentTopology.h"
#include "MeshActorSelectOp.h"
#include "Selection.h"
#include "SelectorHighlight.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cmath>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vtkActor.h>
#include <vtkCell.h>
#include <vtkCellArray.h>
#include <vtkHardwarePicker.h>
#include <vtkIdList.h>
#include <vtkPartitionedDataSet.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

namespace {
constexpr double kPi = 3.14159265358979323846;

/**
 * @brief 用排序后的两个点编号表示无向边，用于查找共享边
 */
struct EdgeKey {
    vtkIdType a {};
    vtkIdType b {};

    bool operator==(const EdgeKey& other) const
    {
        return a == other.a && b == other.b;
    }
};

struct EdgeKeyHash {
    std::size_t operator()(const EdgeKey& key) const
    {
        return std::hash<vtkIdType> {}(key.a) ^ (std::hash<vtkIdType> {}(key.b) << 1);
    }
};

std::vector<vtkIdType>::const_iterator findSelected(
    vtkIdType face_id, const std::vector<vtkIdType>& selections)
{
    return std::find(selections.begin(), selections.end(), face_id);
}

EdgeKey makeEdgeKey(vtkIdType p0, vtkIdType p1)
{
    if (p0 > p1)
        std::swap(p0, p1);
    return { p0, p1 };
}

bool isSelected(vtkIdType face_id, const std::vector<vtkIdType>& selections)
{
    return findSelected(face_id, selections) != selections.end();
}

void addSelected(vtkIdType face_id, std::vector<vtkIdType>& selections)
{
    if (!isSelected(face_id, selections))
        selections.push_back(face_id);
}

void removeSelected(vtkIdType face_id, std::vector<vtkIdType>& selections)
{
    auto it = findSelected(face_id, selections);
    if (it != selections.end())
        selections.erase(it);
}

/**
 * @brief 构建面邻接表，两个面共享同一条无向边时互为邻居
 */
std::vector<std::vector<vtkIdType>> buildFaceAdjacency(vtkPolyData& poly)
{
    std::vector<std::vector<vtkIdType>> adjacency(poly.GetNumberOfCells());
    std::unordered_map<EdgeKey, std::vector<vtkIdType>, EdgeKeyHash> edge_faces;

    // 先记录每条边关联的面，再由边反向建立面邻接关系。
    for (vtkIdType face_id = 0; face_id < poly.GetNumberOfCells(); ++face_id) {
        vtkIdList* point_ids = poly.GetCell(face_id)->GetPointIds();
        vtkIdType point_count = point_ids->GetNumberOfIds();
        for (vtkIdType i = 0; i < point_count; ++i) {
            vtkIdType p0 = point_ids->GetId(i);
            vtkIdType p1 = point_ids->GetId((i + 1) % point_count);
            edge_faces[makeEdgeKey(p0, p1)].push_back(face_id);
        }
    }

    for (const auto& edge_faces_item : edge_faces) {
        const auto& faces = edge_faces_item.second;
        for (std::size_t i = 0; i < faces.size(); ++i) {
            for (std::size_t j = i + 1; j < faces.size(); ++j) {
                adjacency[faces[i]].push_back(faces[j]);
                adjacency[faces[j]].push_back(faces[i]);
            }
        }
    }
    return adjacency;
}

/**
 * @brief 使用 Newell 方法计算多边形面的单位法向，退化面返回零向量
 */
std::array<double, 3> calculateFaceNormal(vtkPolyData& poly, vtkIdType face_id)
{
    std::array<double, 3> normal {};
    vtkIdList* point_ids = poly.GetCell(face_id)->GetPointIds();
    vtkIdType point_count = point_ids->GetNumberOfIds();

    for (vtkIdType i = 0; i < point_count; ++i) {
        double p0[3] {};
        double p1[3] {};
        poly.GetPoint(point_ids->GetId(i), p0);
        poly.GetPoint(point_ids->GetId((i + 1) % point_count), p1);
        normal[0] += (p0[1] - p1[1]) * (p0[2] + p1[2]);
        normal[1] += (p0[2] - p1[2]) * (p0[0] + p1[0]);
        normal[2] += (p0[0] - p1[0]) * (p0[1] + p1[1]);
    }

    double length = std::sqrt(
        normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
    if (length > 0.0) {
        normal[0] /= length;
        normal[1] /= length;
        normal[2] /= length;
    }
    return normal;
}

/**
 * @brief 从种子面开始，沿共享边扩散到法向夹角不超过阈值的连续面
 */
std::vector<vtkIdType> spreadFacesByAngle(
    vtkPolyData& poly, vtkIdType seed_face_id, double angle_deg)
{
    auto adjacency = buildFaceAdjacency(poly);
    std::vector<std::array<double, 3>> normals(poly.GetNumberOfCells());
    for (vtkIdType face_id = 0; face_id < poly.GetNumberOfCells(); ++face_id)
        normals[face_id] = calculateFaceNormal(poly, face_id);

    // 将角度阈值转换成点积阈值，BFS 中无需反复计算 acos。
    angle_deg = std::clamp(angle_deg, 0.0, 180.0);
    double cos_threshold = std::cos(angle_deg * kPi / 180.0);

    std::vector<vtkIdType> region;
    std::queue<vtkIdType> pending;
    std::unordered_set<vtkIdType> visited;
    pending.push(seed_face_id);
    visited.insert(seed_face_id);

    // 每一步比较当前面与相邻面，使阈值表示局部折角。
    while (!pending.empty()) {
        vtkIdType current = pending.front();
        pending.pop();
        region.push_back(current);

        for (vtkIdType next : adjacency[current]) {
            if (!visited.insert(next).second)
                continue;

            const auto& a = normals[current];
            const auto& b = normals[next];
            double dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
            if (dot >= cos_threshold)
                pending.push(next);
        }
    }
    return region;
}
}

FaceSelectorHighlight::FaceSelectorHighlight(vtkRenderer& renderer, vtkPartitionedDataSet& highlight_data,
    unsigned int partition_id, MeshActorSelectOp select_op)
    : renderer_(&renderer)
    , select_op_(std::move(select_op))
    , highlight_data_(&highlight_data)
    , partition_id_(partition_id)
{
    selections_poly_ = vtkSmartPointer<vtkPolyData>::New();
    highlight_data_->SetPartition(partition_id_, selections_poly_);
}

FaceSelectorHighlight::~FaceSelectorHighlight()
{
    highlight_data_->SetPartition(partition_id_, nullptr);
}

SelectionVtk FaceSelectorHighlight::get()
{
    SelectionVtk back_selection;
    back_selection.type = ElementEnum::Face;

    for (const auto& face : selections_) {
        back_selection.ids.push_back(face);
    }

    return back_selection;
}

void FaceSelectorHighlight::clear()
{
    selections_.clear();
    selections_poly_->Initialize();
    highlight_data_->Modified();
}

void FaceSelectorHighlight::select(double posx, double posy)
{
    vtkNew<vtkHardwarePicker> picker;
    picker->PickFromListOn();
    picker->AddPickList(&select_op_.getFaceActor());
    picker->Pick(posx, posy, 0, renderer_);

    vtkIdType picked_cell_id = picker->GetCellId();
    if (picked_cell_id == -1) {
        clear();
        return;
    }

    // 获取选中的 cell
    vtkActor* picked_actor = picker->GetActor();
    assert(picked_actor);
    vtkPolyDataMapper* picked_mapper = vtkPolyDataMapper::SafeDownCast(picked_actor->GetMapper());
    assert(picked_mapper);
    vtkPolyData* picked_poly = picked_mapper->GetInput();
    assert(picked_poly);

    std::vector<vtkIdType> picked_faces { picked_cell_id };
    if (spread_options_.enabled)
        picked_faces = spreadFacesByAngle(*picked_poly, picked_cell_id, spread_options_.angle_deg);

    // 以种子面的状态决定整片区域是加入还是移除，保持原有点击切换语义。
    bool remove_faces = isSelected(picked_cell_id, selections_);
    for (vtkIdType face_id : picked_faces) {
        if (remove_faces)
            removeSelected(face_id, selections_);
        else
            addSelected(face_id, selections_);
    }

    vtkNew<vtkCellArray> cell_array;
    for (const auto& face : selections_) {
        cell_array->InsertNextCell(picked_poly->GetCell(face));
    }

    vtkNew<vtkPolyData> highlight_poly;
    highlight_poly->SetPoints(picked_poly->GetPoints()); // 使用原始数据的点集
    highlight_poly->SetPolys(cell_array); // 设置面单元
    selections_poly_->ShallowCopy(highlight_poly);
    highlight_data_->Modified();
}

void FaceSelectorHighlight::setSpreadOptions(FaceSelectionSpreadOptions options)
{
    spread_options_ = options;
}

void FaceSelectorHighlight::setupHighlightStyle(vtkActor& actor, vtkMapper& mapper)
{
    mapper.SetRelativeCoincidentTopologyPolygonOffsetParameters(0, highlight::POLYGON_UNITS);

    actor.SetMapper(&mapper);
    vtkNew<vtkProperty> prop;
    prop->SetColor(1.0, 0.0, 0.0); // 红色高亮
    prop->SetLineWidth(2.0);
    prop->EdgeVisibilityOn();
    prop->SetEdgeColor(1.0, 0.0, 0.0);
    actor.SetProperty(prop);
}
