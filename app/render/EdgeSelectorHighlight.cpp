#include "CoincidentTopology.h"
#include "MeshActor.h"
#include "MeshActorSelectOp.h"
#include "Selection.h"
#include "SelectorHighlight.h"
#include <optional>
#include <spdlog/spdlog.h>
#include <vtkHardwarePicker.h>
#include <vtkLine.h>
#include <vtkPartitionedDataSet.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
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

    // 边端点的原始id
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
}

EdgeSelectorHighlight::EdgeSelectorHighlight(vtkRenderer& renderer, vtkPartitionedDataSet& highlight_data,
    unsigned int partition_id, MeshActorSelectOp select_op)
    : renderer_(&renderer)
    , select_op_(std::move(select_op))
    , highlight_data_(&highlight_data)
    , partition_id_(partition_id)
{
    highlight_data_->SetPartition(partition_id_, selections_poly_);
}

EdgeSelectorHighlight::~EdgeSelectorHighlight()
{
    highlight_data_->SetPartition(partition_id_, nullptr);
}

void EdgeSelectorHighlight::clear()
{
    clearHighlight();
    selections_.clear();
}

void EdgeSelectorHighlight::clearHighlight()
{
    selections_poly_->Initialize();
    highlight_data_->Modified();
}

void EdgeSelectorHighlight::applyHighlight()
{
    if (selections_.empty())
        return;
    auto edge_poly_data = select_op_.extractEdge(selections_);
    selections_poly_->ShallowCopy(edge_poly_data);
    highlight_data_->Modified();
}

SelectionVtk EdgeSelectorHighlight::get()
{
    SelectionVtk back_selection;
    back_selection.type = ElementEnum::Edge;

    for (const auto& edge : selections_) {
        back_selection.ids.push_back(edge[0]);
        back_selection.ids.push_back(edge[1]);
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

    // 获取选中的CellId （面或者是边）
    vtkIdType picked_cell_id = picker->GetCellId();
    if (picked_cell_id == -1) { // 没选到
        clear();
        return;
    }

    // 获取选中的 cell
    vtkActor* picked_actor = picker->GetActor();
    assert(picked_actor);
    vtkPolyDataMapper* picked_mapper = vtkPolyDataMapper::SafeDownCast(picked_actor->GetMapper());
    assert(picked_mapper);
    vtkPolyData* picked_poly = picked_mapper->GetInput();
    vtkCell* picked_cell = picked_poly->GetCell(picked_cell_id);
    assert(picked_cell);

    // 边端点的原始id
    std::array<vtkIdType, 2> original_id = _find_selected_edge(*picker, *picked_cell, *picked_poly);

    // 检查是否已选中
    auto it = std::find_if(selections_.begin(), selections_.end(),
        [&](const std::array<vtkIdType, 2>& id) {
            return _is_selected(original_id, std::optional<std::array<vtkIdType, 2>>(id));
        });

    if (it != selections_.end()) { // 已选中，取消选中
        selections_.erase(it);
    } else { // 未选中，添加
        selections_.push_back(original_id);
    }

    applyHighlight();
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
