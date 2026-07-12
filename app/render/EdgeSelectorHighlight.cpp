#include "MeshActor.h"
#include "MeshActorSelectOp.h"
#include "Selection.h"
#include "SelectorHighlight.h"
#include <optional>
#include <spdlog/spdlog.h>
#include <vtkDataSetMapper.h>
#include <vtkHardwarePicker.h>
#include <vtkLine.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

namespace {
std::array<vtkIdType, 2> _find_selected_edge(vtkHardwarePicker& picker, vtkCell& picked_cell, vtkPolyData& pickedPoly)
{
    if (picked_cell.GetCellType() == VTK_LINE)
        return { picked_cell.GetPointId(0), picked_cell.GetPointId(1) };

    double pPos[3] {};
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

void _cancel_highlight(vtkDataSetMapper* selectedMapper)
{
    vtkNew<vtkPolyData> empty;
    selectedMapper->SetInputData(empty);
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

EdgeSelectorHighlight::EdgeSelectorHighlight(vtkRenderer& renderer, vtkActor& highlight_actor, MeshActorSelectOp select_op)
    : renderer_(&renderer)
    , highlight_actor_(&highlight_actor)
    , select_op_(std::move(select_op))
{
    selected_mapper_->SetInputData(vtkPolyData::New());

    if (highlight_actor_) {
        highlight_actor_->SetMapper(selected_mapper_);
        vtkNew<vtkProperty> prop;
        prop->SetColor(MeshActor::colors->GetColor3d("red").GetData());
        prop->SetLineWidth(5);
        highlight_actor_->SetProperty(prop);
    }
}

EdgeSelectorHighlight::~EdgeSelectorHighlight()
{
    clear();
}

void EdgeSelectorHighlight::clear()
{
    _cancel_highlight(selected_mapper_);
    selections_.clear();
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
    vtkIdType pickedCellId = picker->GetCellId();
    if (pickedCellId == -1) { // 没选到
        clear(); 
        return;
    }
    
    // 获取选中的 cell
    vtkActor* pickedActor = picker->GetActor();
    assert(pickedActor);
    vtkPolyDataMapper* pickedMapper = vtkPolyDataMapper::SafeDownCast(pickedActor->GetMapper());
    assert(pickedMapper);
    vtkPolyData* pickedPoly = pickedMapper->GetInput();
    vtkCell* pickedCell = pickedPoly->GetCell(pickedCellId);
    assert(pickedCell);
    
    // 边端点的原始id
    std::array<vtkIdType, 2> original_id = _find_selected_edge(*picker, *pickedCell, *pickedPoly);

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

    auto edge_poly_data = select_op_.extractEdge(selections_);
    selected_mapper_->SetInputData(edge_poly_data);
}
