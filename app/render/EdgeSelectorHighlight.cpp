#include "MeshAreaPick.h"
#include "CoincidentTopology.h"
#include "MeshActor.h"
#include "MeshActorSelectOp.h"
#include "MeshIdQuery.h"
#include "Selection.h"
#include "SelectorHighlight.h"
#include <algorithm>
#include <optional>
#include <unordered_set>
#include <spdlog/spdlog.h>
#include <utility>
#include <vtkCell.h>
#include <vtkCellType.h>
#include <vtkHardwarePicker.h>
#include <vtkIdTypeArray.h>
#include <vtkLine.h>
#include <vtkPartitionedDataSet.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

namespace {
std::array<vtkIdType, 2> _find_selected_edge(vtkHardwarePicker& picker, vtkCell& picked_cell, vtkPolyData& pickedPoly)
{
    if (picked_cell.GetCellType() == VTK_LINE)
        return { picked_cell.GetPointId(0), picked_cell.GetPointId(1) };

    double pPos[3] { };
    picker.GetPCoords(pPos);

    vtkNew<vtkIdList> cellIds;
    picked_cell.CellBoundary(0, pPos, cellIds);

    // 边端点的局部点 id
    std::array<vtkIdType, 2> original_id;
    auto point_id_array = vtkIdTypeArray::SafeDownCast(pickedPoly.GetPointData()->GetArray("vtkOriginalPointIds"));
    assert(point_id_array);
    original_id[0] = point_id_array->GetValue(cellIds->GetId(0));
    original_id[1] = point_id_array->GetValue(cellIds->GetId(1));

    return { original_id[0], original_id[1] };
}

bool _is_selected(std::array<vtkIdType, 2> v_local_id, const std::optional<std::array<vtkIdType, 2>>& selection)
{
    if (selection) {
        // 选中的边点id，交换意义下对应相同
        const std::array<vtkIdType, 2>& selected1 = *selection;
        const std::array<vtkIdType, 2>& selected2 = v_local_id;
        return selected1[0] == selected2[0] && selected1[1] == selected2[1]
            || selected1[0] == selected2[1] && selected1[1] == selected2[0];
    }
    return false;
}

//! @brief 无序化端点对（min,max）的哈希，供框选端点对集合用
struct EdgePairHash {
    std::size_t operator()(const std::pair<vtkIdType, vtkIdType>& p) const
    {
        std::size_t h1 = std::hash<vtkIdType> {}(p.first);
        std::size_t h2 = std::hash<vtkIdType> {}(p.second);
        std::size_t seed = h1;
        seed ^= h2 + 0x9e3779b9U + (seed << 6) + (seed >> 2);
        return seed;
    }
};
}

EdgeSelectorHighlight::EdgeSelectorHighlight(vtkRenderer& renderer, vtkPartitionedDataSet& highlight_data,
    unsigned int partition_id, MeshActorSelectOp select_op,
    Index component_id, const IMeshIdQuery* id_query)
    : renderer_(&renderer)
    , select_op_(std::move(select_op))
    , highlight_data_(&highlight_data)
    , partition_id_(partition_id)
    , component_id_(component_id)
    , id_query_(id_query)
{
    highlight_data_->SetPartition(partition_id_, selections_poly_);
}

EdgeSelectorHighlight::~EdgeSelectorHighlight()
{
    highlight_data_->SetPartition(partition_id_, nullptr);
}

void EdgeSelectorHighlight::clear()
{
    disableHighlight();
    selections_.clear();
}

void EdgeSelectorHighlight::disableHighlight()
{
    selections_poly_->Initialize();
    highlight_data_->Modified();
}

void EdgeSelectorHighlight::enableHighlight()
{
    if (selections_.empty())
        return;

    // 高亮仍按端点对画线
    std::vector<std::array<vtkIdType, 2>> highlight_edges;
    highlight_edges.reserve(selections_.size());
    for (const auto& e : selections_)
        highlight_edges.push_back(e.endpoints);

    auto edge_poly_data = select_op_.extractEdge(highlight_edges);
    selections_poly_->ShallowCopy(edge_poly_data);
    highlight_data_->Modified();
}

SelectionVtk EdgeSelectorHighlight::get()
{
    SelectionVtk back_selection;
    back_selection.type = ElementEnum::Edge;

    // 稳定 id 语义：回传稳定局部边 id（跨拓扑编辑有效）；
    // id 查询缺失（防御路径）时回退为两个端点 id 顺次排列，端点在出口经 id 查询桥统一换算全局点 id。
    for (const auto& edge : selections_) {
        if (edge.edge_id >= 0) {
            back_selection.ids.push_back(edge.edge_id);
        } else {
            const Index gid0 = id_query_ ? id_query_->pointGlobalId(component_id_, static_cast<Index>(edge.endpoints[0])) : -1;
            const Index gid1 = id_query_ ? id_query_->pointGlobalId(component_id_, static_cast<Index>(edge.endpoints[1])) : -1;
            if (gid0 >= 0 && gid1 >= 0) {
                back_selection.ids.push_back(gid0);
                back_selection.ids.push_back(gid1);
            }
        }
    }

    return back_selection;
}

// 用词：picker的picked cell -> selector的selected cell
void EdgeSelectorHighlight::select(double posx, double posy)
{
    vtkNew<vtkHardwarePicker> picker;
    picker->PickFromListOn();
    picker->AddPickList(&select_op_.getEdgeActor());
    picker->AddPickList(&select_op_.getFaceActor());
    picker->AddPickList(&select_op_.getSolidActor());
    picker->Pick(posx, posy, 0, renderer_);

    select(posx, posy, picker.GetPointer(), picker->GetActor(),
        picker->GetCellId(), picker->GetPointId());
}

void EdgeSelectorHighlight::select(double posx, double posy,
    vtkHardwarePicker* picker, vtkActor* picked_actor,
    vtkIdType picked_cell_id, vtkIdType /*picked_point_id*/)
{
    if (!picked_actor || picked_cell_id == -1) {
        clear();
        return;
    }

    vtkPolyDataMapper* picked_mapper = vtkPolyDataMapper::SafeDownCast(picked_actor->GetMapper());
    assert(picked_mapper);
    vtkPolyData* picked_poly = picked_mapper->GetInput();
    vtkCell* picked_cell = picked_poly->GetCell(picked_cell_id);
    assert(picked_cell);

    // 边端点的原始id
    std::array<vtkIdType, 2> original_id = _find_selected_edge(*picker, *picked_cell, *picked_poly);

    // 经模型层统一边表解析稳定局部边 id；id 查询缺失（防御路径）时记 -1 按端点对兜底
    Index edge_id = -1;
    if (id_query_) {
        auto resolved = id_query_->findEdgeByEndpoints(component_id_,
            static_cast<Index>(original_id[0]), static_cast<Index>(original_id[1]));
        if (resolved)
            edge_id = *resolved;
    }

    // 检查是否已选中（端点对在交换意义下相同即为同一条边）
    auto it = std::find_if(selections_.begin(), selections_.end(),
        [&](const SelectedEdge& e) {
            return _is_selected(original_id, std::optional<std::array<vtkIdType, 2>>(e.endpoints));
        });

    if (it != selections_.end()) { // 已选中，取消选中
        selections_.erase(it);
    } else { // 未选中，添加
        selections_.push_back({ original_id, edge_id });
    }

    enableHighlight();
}

void EdgeSelectorHighlight::setupHighlightStyle(vtkActor& actor, vtkMapper& mapper)
{
    mapper.SetRelativeCoincidentTopologyLineOffsetParameters(0, highlight::LINE_UNITS);

    actor.SetMapper(&mapper);
    vtkNew<vtkProperty> prop;
    prop->SetColor(MeshActor::colors->GetColor3d("red").GetData());
    prop->SetLineWidth(5);
    actor.SetProperty(prop);
}

void EdgeSelectorHighlight::selectArea(
    const std::unordered_map<vtkProp*, std::unordered_set<vtkIdType>>& hits,
    int xmin, int ymin, int xmax, int ymax)
{
    // 与点选对齐：从 face/edge/solid 三个 actor 的命中 cell 派生边并合并端点对。
    // face CELLS 主路径健壮（面表面有 z 遮挡）；edge actor 补独立/物化边；solid 表面补体网格表面边。
    std::unordered_set<std::pair<vtkIdType, vtkIdType>, EdgePairHash> picked_edge_set;

    // 命中查找：取本 actor 在 hits 中的命中集合（无命中返回 nullptr）
    auto hit_of = [&](vtkProp* prop) -> const std::unordered_set<vtkIdType>* {
        auto it = hits.find(prop);
        return it == hits.end() ? nullptr : &it->second;
    };

    // 从单个 actor 的命中 cell 派生"局部点 id 端点对"
    auto derive_edges = [&](vtkActor* target, vtkPolyData* poly,
        const std::unordered_set<vtkIdType>* picked) {
        if (!target || !poly || !picked || picked->empty())
            return;
        if (poly->GetNumberOfCells() == 0)
            return;

        // 裁剪链上点 id 会重排：vTK 拾取后经 vtkOriginalPointIds 还原组件局部点 id
        auto* orig_pt_ids = vtkIdTypeArray::SafeDownCast(
            poly->GetPointData()->GetArray("vtkOriginalPointIds"));
        for (vtkIdType cell_id : *picked) {
            if (cell_id < 0 || cell_id >= poly->GetNumberOfCells())
                continue;
            vtkCell* cell = poly->GetCell(cell_id);
            if (!cell)
                continue;
            // 线 cell 本身就是边；面 cell 经 GetEdge 拆边
            const bool is_line = cell->GetCellType() == VTK_LINE;
            for (int e = 0; e < (is_line ? 1 : cell->GetNumberOfEdges()); ++e) {
                vtkIdList* pt_ids = nullptr;
                if (is_line) {
                    pt_ids = cell->GetPointIds();
                } else {
                    vtkCell* edge_cell = cell->GetEdge(e);
                    if (!edge_cell)
                        continue;
                    pt_ids = edge_cell->GetPointIds();
                }
                if (!pt_ids || pt_ids->GetNumberOfIds() < 2)
                    continue;
                const vtkIdType rp0 = pt_ids->GetId(0);
                const vtkIdType rp1 = pt_ids->GetId(1);
                double pp0[3], pp1[3];
                poly->GetPoint(rp0, pp0);
                poly->GetPoint(rp1, pp1);
                // 线段与框相交即选中（端点落入或穿过矩形均算），无需框到端点
                if (!area_pick::isScreenSegmentIntersectsBox(renderer_, target,
                        pp0[0], pp0[1], pp0[2], pp1[0], pp1[1], pp1[2],
                        xmin, ymin, xmax, ymax))
                    continue;
                vtkIdType p0 = rp0, p1 = rp1;
                if (orig_pt_ids) {
                    if (rp0 < 0 || rp0 >= orig_pt_ids->GetNumberOfTuples()
                        || rp1 < 0 || rp1 >= orig_pt_ids->GetNumberOfTuples())
                        continue;
                    p0 = orig_pt_ids->GetValue(rp0);
                    p1 = orig_pt_ids->GetValue(rp1);
                }
                if (p0 < 0 || p1 < 0)
                    continue;
                picked_edge_set.insert({ std::min(p0, p1), std::max(p0, p1) });
            }
        }
    };

    // 1) face 表面（主路径）
    auto* face_actor = vtkActor::SafeDownCast(&select_op_.getFaceActor());
    if (auto* face_poly = face_actor ? vtkPolyData::SafeDownCast(
            vtkPolyDataMapper::SafeDownCast(face_actor->GetMapper())->GetInput()) : nullptr) {
        derive_edges(face_actor, face_poly, hit_of(&select_op_.getFaceActor()));
    }

    // 2) edge actor：独立/物化边（线 cell）
    auto* edge_actor = vtkActor::SafeDownCast(&select_op_.getEdgeActor());
    if (auto* edge_poly = edge_actor ? vtkPolyData::SafeDownCast(
            vtkPolyDataMapper::SafeDownCast(edge_actor->GetMapper())->GetInput()) : nullptr) {
        if (edge_poly->GetNumberOfLines() > 0)
            derive_edges(edge_actor, edge_poly, hit_of(&select_op_.getEdgeActor()));
    }

    // 3) solid 表面：体网格表面边（face 无 cell 时是主要来源）
    auto* solid_actor = vtkActor::SafeDownCast(&select_op_.getSolidActor());
    if (auto* solid_poly = solid_actor ? vtkPolyData::SafeDownCast(
            vtkPolyDataMapper::SafeDownCast(solid_actor->GetMapper())->GetInput()) : nullptr) {
        derive_edges(solid_actor, solid_poly, hit_of(&select_op_.getSolidActor()));
    }

    spdlog::debug("[EdgeArea] picked_edge_set.size()={}", picked_edge_set.size());
    if (picked_edge_set.empty())
        return;

    // 端点对 -> 稳定局部 edge id（与点选路径一致）
    std::vector<SelectedEdge> picked_edges;
    picked_edges.reserve(picked_edge_set.size());
    for (const auto& key : picked_edge_set) {
        Index edge_id = -1;
        if (id_query_) {
            auto resolved = id_query_->findEdgeByEndpoints(component_id_,
                static_cast<Index>(key.first), static_cast<Index>(key.second));
            if (resolved)
                edge_id = *resolved;
        }
        picked_edges.push_back({ { key.first, key.second }, edge_id });
    }

    // 框选恒为替换：manager 已先清空，命中即本组件的新选择（端点对相同即同一条边，set）
    auto match_selected = [&](const SelectedEdge& e) {
        return std::find_if(selections_.begin(), selections_.end(),
            [&](const SelectedEdge& s) {
                return _is_selected(e.endpoints, std::optional<std::array<vtkIdType, 2>>(s.endpoints));
            });
    };

    for (const auto& e : picked_edges) {
        auto it = match_selected(e);
        if (it == selections_.end())
            selections_.push_back(e);
    }

    enableHighlight();
}
