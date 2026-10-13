#include "MeshAreaPick.h"
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
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vtkActor.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkDataObject.h>
#include <vtkHardwarePicker.h>
#include <vtkIdList.h>
#include <vtkIdTypeArray.h>
#include <vtkPartitionedDataSet.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

namespace {
constexpr double kPi = 3.14159265358979323846;

/** @brief 将拾取到的渲染面下标转换为原始面 ID，避免隐藏或裁剪重排后选择身份改变。 */
vtkIdType getOriginalFaceId(vtkPolyData& poly, vtkIdType render_id)
{
    auto* face_ids = vtkIdTypeArray::SafeDownCast(poly.GetCellData()->GetArray("PrecessFaceIds"));
    if (face_ids && render_id >= 0 && render_id < face_ids->GetNumberOfTuples())
        return face_ids->GetValue(render_id);
    return render_id;
}

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

/**
 * @brief 组合无向边两个端点的哈希，降低规则点编号下的冲突
 */
struct EdgeKeyHash {
    std::size_t operator()(const EdgeKey& key) const
    {
        std::size_t h1 = std::hash<vtkIdType> {}(key.a);
        std::size_t h2 = std::hash<vtkIdType> {}(key.b);
        return h1 ^ (h2 + static_cast<std::size_t>(0x9e3779b9U) + (h1 << 6) + (h1 >> 2));
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

/**
 * @brief 批量加入面编号，通过集合避免逐面线性查重
 */
void addSelected(const std::vector<vtkIdType>& face_ids, std::vector<vtkIdType>& selections)
{
    std::unordered_set<vtkIdType> selected_ids(selections.begin(), selections.end());
    for (vtkIdType face_id : face_ids) {
        if (selected_ids.insert(face_id).second)
            selections.push_back(face_id);
    }
}

/**
 * @brief 批量移除面编号，保持其余选择面的原有顺序
 */
void removeSelected(const std::vector<vtkIdType>& face_ids, std::vector<vtkIdType>& selections)
{
    std::unordered_set<vtkIdType> removed_ids(face_ids.begin(), face_ids.end());
    auto first_removed = std::remove_if(selections.begin(), selections.end(),
        [&](vtkIdType face_id) { return removed_ids.find(face_id) != removed_ids.end(); });
    selections.erase(first_removed, selections.end());
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
        vtkIdType point_count = 0;
        const vtkIdType* point_ids = nullptr;
        poly.GetCellPoints(face_id, point_count, point_ids);
        for (vtkIdType i = 0; i < point_count; ++i) {
            vtkIdType p0 = point_ids[i];
            vtkIdType p1 = point_ids[(i + 1) % point_count];
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
    vtkIdType point_count = 0;
    const vtkIdType* point_ids = nullptr;
    poly.GetCellPoints(face_id, point_count, point_ids);

    for (vtkIdType i = 0; i < point_count; ++i) {
        double p0[3] {};
        double p1[3] {};
        poly.GetPoint(point_ids[i], p0);
        poly.GetPoint(point_ids[(i + 1) % point_count], p1);
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
    const std::vector<std::vector<vtkIdType>>& adjacency,
    const std::vector<std::array<double, 3>>& normals,
    vtkIdType seed_face_id, double angle_deg)
{
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
            if (visited.find(next) != visited.end())
                continue;

            const auto& a = normals[current];
            const auto& b = normals[next];
            double dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
            if (dot >= cos_threshold) {
                visited.insert(next);
                pending.push(next);
            }
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

    // 选择在拾取时已转换为原始面 ID，隐藏后不依赖当前可见面表。
    for (vtkIdType face_id : selections_)
        back_selection.ids.push_back(static_cast<Index>(face_id));

    return back_selection;
}

void FaceSelectorHighlight::clear()
{
    disableHighlight();
    selections_.clear();
}

void FaceSelectorHighlight::disableHighlight()
{
    selections_poly_->Initialize();
    highlight_data_->Modified();
}

void FaceSelectorHighlight::enableHighlight()
{
    if (selections_.empty()) {
        disableHighlight();
        return;
    }
    auto* face_actor = vtkActor::SafeDownCast(&select_op_.getFaceActor());
    if (!face_actor)
        return;
    auto* mapper = vtkPolyDataMapper::SafeDownCast(face_actor->GetMapper());
    if (!mapper)
        return;
    // 显隐过滤会改变当前输出，先更新再按原始面 ID 重建可见部分高亮。
    mapper->Update();
    auto* poly = mapper->GetInput();
    if (!poly)
        return;

    auto* face_ids = vtkIdTypeArray::SafeDownCast(poly->GetCellData()->GetArray("PrecessFaceIds"));
    vtkNew<vtkCellArray> cell_array;
    vtkNew<vtkIdList> render_ids;
    for (vtkIdType face_id : selections_) {
        render_ids->Reset();
        // 按原始面 ID 查找当前渲染单元；隐藏或被裁剪剔除的面没有匹配项。
        if (face_ids)
            face_ids->LookupTypedValue(face_id, render_ids);
        else if (face_id >= 0 && face_id < poly->GetNumberOfCells())
            render_ids->InsertNextId(face_id);
        for (vtkIdType i = 0; i < render_ids->GetNumberOfIds(); ++i) {
            vtkIdType point_count = 0;
            const vtkIdType* point_ids = nullptr;
            poly->GetCellPoints(render_ids->GetId(i), point_count, point_ids);
            cell_array->InsertNextCell(point_count, point_ids);
        }
    }

    vtkNew<vtkPolyData> highlight_poly;
    highlight_poly->SetPoints(poly->GetPoints());
    highlight_poly->SetPolys(cell_array);
    selections_poly_->ShallowCopy(highlight_poly);
    highlight_data_->Modified();
}

void FaceSelectorHighlight::updateSpreadCache(vtkPolyData& poly)
{
    vtkMTimeType mtime = poly.GetMTime();
    if (spread_cache_.poly_data == &poly && spread_cache_.mtime == mtime)
        return;

    spread_cache_.poly_data = &poly;
    spread_cache_.mtime = mtime;
    spread_cache_.adjacency = buildFaceAdjacency(poly);
    spread_cache_.normals.resize(poly.GetNumberOfCells());
    for (vtkIdType face_id = 0; face_id < poly.GetNumberOfCells(); ++face_id)
        spread_cache_.normals[face_id] = calculateFaceNormal(poly, face_id);
}

void FaceSelectorHighlight::select(double posx, double posy)
{
    // 兼容路径：自行构建 picker 并拾取；生产路径由 MeshSelectManager 预拾后调下方的 picker 重载。
    vtkNew<vtkHardwarePicker> picker;
    picker->PickFromListOn();
    picker->AddPickList(&select_op_.getFaceActor());
    picker->Pick(posx, posy, 0, renderer_);

    select(posx, posy, picker.GetPointer(), picker->GetActor(),
        picker->GetCellId(), picker->GetPointId(), SelectOp::Toggle);
}

void FaceSelectorHighlight::select(double posx, double posy,
    vtkHardwarePicker* /*picker*/, vtkActor* picked_actor,
    vtkIdType picked_cell_id, vtkIdType /*picked_point_id*/, SelectOp op)
{
    if (!picked_actor || picked_cell_id == -1) {
        if (op == SelectOp::Toggle)
            clear();
        return;
    }

    vtkPolyDataMapper* picked_mapper = vtkPolyDataMapper::SafeDownCast(picked_actor->GetMapper());
    assert(picked_mapper);
    vtkPolyData* picked_poly = picked_mapper->GetInput();
    assert(picked_poly);

    std::vector<vtkIdType> picked_faces { picked_cell_id };
    if (spread_options_.enabled) {
        updateSpreadCache(*picked_poly);
        picked_faces = spreadFacesByAngle(spread_cache_.adjacency,
            spread_cache_.normals, picked_cell_id, spread_options_.angle_deg);
    }

    // 邻接扩散仍使用当前可见面的渲染下标；只有保存选择时转换为稳定身份。
    for (auto& face_id : picked_faces)
        face_id = getOriginalFaceId(*picked_poly, face_id);

    // 按 op 决定整片区域（含角度扩散）的选中语义：
    switch (op) {
    case SelectOp::Append:
        addSelected(picked_faces, selections_);
        break;
    case SelectOp::Remove:
        removeSelected(picked_faces, selections_);
        break;
    case SelectOp::Toggle:
        if (isSelected(picked_faces.front(), selections_))
            removeSelected(picked_faces, selections_);
        else
            addSelected(picked_faces, selections_);
        break;
    }

    enableHighlight();
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

void FaceSelectorHighlight::selectArea(
    const std::unordered_map<vtkProp*, std::unordered_set<vtkIdType>>& hits,
    int /*xmin*/, int /*ymin*/, int /*xmax*/, int /*ymax*/, SelectOp op)
{
    // face actor 的命中即面 render cell id（MeshSelectManager 一次多 actor 拾取、已清空后分发）
    auto it = hits.find(&select_op_.getFaceActor());
    if (it == hits.end())
        return;
    const auto& picked = it->second;

    spdlog::debug("[FaceArea] picked.size()={}", picked.size());
    if (picked.empty())
        return;

    // 按 op 对框内命中的面做集合运算：Append->并集；Remove->差集；Toggle->对称差。
    std::unordered_set<vtkIdType> cur(selections_.begin(), selections_.end());
    auto* face_actor = vtkActor::SafeDownCast(&select_op_.getFaceActor());
    auto* mapper = face_actor ? vtkPolyDataMapper::SafeDownCast(face_actor->GetMapper()) : nullptr;
    auto* poly = mapper ? mapper->GetInput() : nullptr;
    if (!poly)
        return;
    // 框选也在集合运算前统一转换，避免与保留的隐藏面混用两种下标。
    std::unordered_set<vtkIdType> picked_set;
    for (vtkIdType render_id : picked)
        picked_set.insert(getOriginalFaceId(*poly, render_id));
    std::unordered_set<vtkIdType> result;
    switch (op) {
    case SelectOp::Append:
        result = cur;
        result.insert(picked_set.begin(), picked_set.end());
        break;
    case SelectOp::Remove:
        for (auto id : cur)
            if (picked_set.find(id) == picked_set.end())
                result.insert(id);
        break;
    case SelectOp::Toggle:
        for (auto id : cur)
            if (picked_set.find(id) == picked_set.end())
                result.insert(id);
        for (auto id : picked_set)
            if (cur.find(id) == cur.end())
                result.insert(id);
        break;
    }
    selections_.assign(result.begin(), result.end());
    enableHighlight();
}
