#include "MeshAreaPick.h"
#include "CoincidentTopology.h"
#include "MeshActorSelectOp.h"
#include "MeshIdQuery.h"
#include "Selection.h"
#include "SelectorHighlight.h"
#include <optional>
#include <unordered_set>
#include <spdlog/spdlog.h>
#include <vtkActor.h>
#include <vtkCell.h>
#include <vtkDataSet.h>
#include <vtkExtractSelection.h>
#include <vtkGeometryFilter.h>
#include <vtkHardwarePicker.h>
#include <vtkMapper.h>
#include <vtkPartitionedDataSet.h>
#include <vtkPointData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

namespace {
void _cancel_highlight(vtkIdTypeArray* selected_ids)
{
    selected_ids->SetNumberOfValues(0);
    selected_ids->Modified();
}

vtkIdType _is_selected(vtkIdType new_vertex, const vtkIdTypeArray& selected_ids_)
{
    vtkIdTypeArray& selected_ids = const_cast<vtkIdTypeArray&>(selected_ids_);
    vtkIdType id_idx = selected_ids.LookupTypedValue(new_vertex);
    return id_idx;
}
}

VertexSelectorHighlight::VertexSelectorHighlight(vtkRenderer& renderer, vtkPartitionedDataSet& highlight_data,
    unsigned int partition_id, MeshActorSelectOp select_op,
    Index component_id, const IMeshIdQuery* id_query)
    : renderer_(&renderer)
    , select_op_(std::move(select_op))
    , highlight_data_(&highlight_data)
    , partition_id_(partition_id)
    , component_id_(component_id)
    , id_query_(id_query)
{
    this->selected_ids_->SetNumberOfTuples(1);
    this->selected_ids_->SetNumberOfValues(0);

    extract_filter_ = select_op_.extractVertex(this->selected_ids_);
    geom_filter_->SetInputConnection(extract_filter_->GetOutputPort());
    geom_filter_->Update();
    highlight_data_->SetPartition(partition_id_, geom_filter_->GetOutput());
}

VertexSelectorHighlight::~VertexSelectorHighlight()
{
    highlight_data_->SetPartition(partition_id_, nullptr);
}

SelectionVtk VertexSelectorHighlight::get()
{
    SelectionVtk back_selection;
    back_selection.type = ElementEnum::Vertex;
    // 选中状态存局部点 id（高亮提取键空间），出口经 id 查询桥统一换算为全局点 id（跨层身份）
    for (vtkIdType i = 0; i < selected_ids_->GetNumberOfValues(); ++i) {
        const auto local = static_cast<Index>(selected_ids_->GetValue(i));
        const Index gid = id_query_ ? id_query_->pointGlobalId(component_id_, local) : -1;
        if (gid >= 0)
            back_selection.ids.push_back(gid);
    }
    return back_selection;
}

void VertexSelectorHighlight::clear()
{
    _cancel_highlight(this->selected_ids_);
    disableHighlight();
}

void VertexSelectorHighlight::disableHighlight()
{
    highlight_data_->SetPartition(partition_id_, nullptr);
    highlight_data_->Modified();
}

void VertexSelectorHighlight::enableHighlight()
{
    if (selected_ids_->GetNumberOfValues() == 0) {
        disableHighlight();
        return;
    }
    geom_filter_->Update();
    highlight_data_->SetPartition(partition_id_, geom_filter_->GetOutput());
    highlight_data_->Modified();
}

void VertexSelectorHighlight::select(double posx, double posy)
{
    // 兼容路径：自行构建 picker 并拾取；生产路径由 MeshSelectManager 预拾后调下方的 picker 重载。
    vtkNew<vtkHardwarePicker> picker;
    picker->SnapToMeshPointOn(); // 启用贴近网格点
    picker->SetPixelTolerance(5); // 设置点拾取像素容差
    picker->PickFromListOn();
    picker->AddPickList(&select_op_.getSolidActor());
    picker->AddPickList(&select_op_.getFaceActor());
    picker->AddPickList(&select_op_.getEdgeActor());
    picker->Pick(posx, posy, 0, renderer_);

    select(posx, posy, picker.GetPointer(), picker->GetActor(),
        picker->GetCellId(), picker->GetPointId());
}

void VertexSelectorHighlight::select(double posx, double posy,
    vtkHardwarePicker* picker, vtkActor* picked_actor,
    vtkIdType /*picked_cell_id*/, vtkIdType picked_point_id)
{
    // 未命中点（SnapToMeshPoint 取不到）按原语义保留已有选择
    if (!picker || !picked_actor || picked_point_id == -1) {
        spdlog::debug("VertexSelectorHighlight::select: no point picked.");
        return;
    }
    vtkDataSet* picked_data_set = picker->GetDataSet();

    // 获取对应的点id selected_vertex_id
    auto vertex_id_array = vtkIdTypeArray::SafeDownCast(picked_data_set->GetPointData()->GetArray("vtkOriginalPointIds"));
    if (!vertex_id_array) {
        clear();
        spdlog::debug("Picked point id: {}, no vertex id array found.", picked_point_id);
        return;
    }
    vtkIdType selected_vertex_id = vertex_id_array->GetValue(picked_point_id);

    // 检查该点是否已经被选中
    vtkIdType id_idx = _is_selected(selected_vertex_id, *this->selected_ids_);
    if (id_idx >= 0) {
        // 已选中，取消选中
        selected_ids_->RemoveTuple(id_idx);
        spdlog::debug("VertexSelectorHighlight::select: point {} deselected.", selected_vertex_id);
    } else {
        // 未选中，添加选中
        selected_ids_->InsertNextValue(selected_vertex_id);
        selected_ids_->ClearLookup(); // 清除查找缓存，确保下一次查找正确
        spdlog::debug("VertexSelectorHighlight::select: point {} selected.", selected_vertex_id);
    }

    selected_ids_->Modified();
    enableHighlight();
}

void VertexSelectorHighlight::setupHighlightStyle(vtkActor& actor, vtkMapper& mapper)
{
    mapper.SetRelativeCoincidentTopologyPointOffsetParameter(highlight::POINT_UNITS);

    actor.SetMapper(&mapper);
    vtkNew<vtkProperty> prop;
    prop->SetColor(1.0, 0.0, 0.0);
    prop->SetPointSize(6.0);
    actor.SetProperty(prop);
}

void VertexSelectorHighlight::selectArea(
    const std::unordered_map<vtkProp*, std::unordered_set<vtkIdType>>& hits,
    int xmin, int ymin, int xmax, int ymax)
{
    // 与点选对齐：从 face/edge/solid 三个 actor 的命中 cell 派生点并合并（一次多 actor 拾取结果）。
    std::unordered_set<vtkIdType> local_ids;

    // 命中查找：取本 actor 在 hits 中的命中集合（无命中返回 nullptr）
    auto hit_of = [&](vtkProp* prop) -> const std::unordered_set<vtkIdType>* {
        auto it = hits.find(prop);
        return it == hits.end() ? nullptr : &it->second;
    };

    // 从单个 actor 的命中 cell 派生"组件局部点 id"
    auto derive_points = [&](vtkActor* target, vtkPolyData* poly,
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
            for (int i = 0; i < cell->GetNumberOfPoints(); ++i) {
                vtkIdType rp = cell->GetPointId(i);
                if (rp < 0 || rp >= poly->GetNumberOfPoints())
                    continue;
                double p[3];
                poly->GetPoint(rp, p);
                // 像素在框内才保留（demo POINTS 语义）
                if (!area_pick::isWorldPointInScreenBox(renderer_, target,
                        p[0], p[1], p[2], xmin, ymin, xmax, ymax))
                    continue;
                const vtkIdType lp = (orig_pt_ids && rp < orig_pt_ids->GetNumberOfTuples())
                    ? orig_pt_ids->GetValue(rp)
                    : rp;
                if (lp >= 0)
                    local_ids.insert(lp);
            }
        }
    };

    // 1) face 表面（主路径）
    auto* face_actor = vtkActor::SafeDownCast(&select_op_.getFaceActor());
    if (auto* face_poly = face_actor ? vtkPolyData::SafeDownCast(
            vtkPolyDataMapper::SafeDownCast(face_actor->GetMapper())->GetInput()) : nullptr) {
        derive_points(face_actor, face_poly, hit_of(&select_op_.getFaceActor()));
    }

    // 2) edge actor：独立/物化边上的点（线 cell 端点）
    auto* edge_actor = vtkActor::SafeDownCast(&select_op_.getEdgeActor());
    if (auto* edge_poly = edge_actor ? vtkPolyData::SafeDownCast(
            vtkPolyDataMapper::SafeDownCast(edge_actor->GetMapper())->GetInput()) : nullptr) {
        if (edge_poly->GetNumberOfLines() > 0)
            derive_points(edge_actor, edge_poly, hit_of(&select_op_.getEdgeActor()));
    }

    // 3) solid 表面：体网格表面点（face 无 cell 时是主要来源）
    auto* solid_actor = vtkActor::SafeDownCast(&select_op_.getSolidActor());
    if (auto* solid_poly = solid_actor ? vtkPolyData::SafeDownCast(
            vtkPolyDataMapper::SafeDownCast(solid_actor->GetMapper())->GetInput()) : nullptr) {
        derive_points(solid_actor, solid_poly, hit_of(&select_op_.getSolidActor()));
    }

    spdlog::debug("[VertexArea] local_ids.size()={}", local_ids.size());
    if (local_ids.empty())
        return;

    // 框选恒为替换：manager 已先清空，命中即本组件的新选择（set）
    selected_ids_->ClearLookup();
    for (vtkIdType v : local_ids) {
        if (_is_selected(v, *selected_ids_) < 0)
            selected_ids_->InsertNextValue(v);
    }
    selected_ids_->Modified();
    enableHighlight();
}
