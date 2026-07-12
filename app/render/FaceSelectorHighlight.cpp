#include "MeshActorSelectOp.h"
#include "Selection.h"
#include "SelectorHighlight.h"
#include <spdlog/spdlog.h>
#include <vtkDataSetMapper.h>
#include <vtkHardwarePicker.h>
#include <vtkPolyData.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

namespace {
void _cancel_highlight(vtkDataSetMapper* selectedMapper)
{
    vtkNew<vtkPolyData> empty;
    selectedMapper->SetInputData(empty);
}

std::vector<vtkIdType>::const_iterator _find_selected(vtkIdType new_face_id, const std::vector<vtkIdType>& selections)
{
    return std::find_if(selections.begin(), selections.end(),
        [&](const vtkIdType& id) {
            return id == new_face_id;
        });
}
}

FaceSelectorHighlight::FaceSelectorHighlight(vtkRenderer& renderer, vtkActor& highlight_actor, MeshActorSelectOp select_op)
    : renderer_(&renderer)
    , highlight_actor_(&highlight_actor)
    , select_op_(std::move(select_op))
{
    selected_mapper_->SetInputData(vtkPolyData::New());
    selected_mapper_->SetRelativeCoincidentTopologyPolygonOffsetParameters(0, -0.5);

    if (highlight_actor_) {
        highlight_actor_->SetMapper(selected_mapper_);
        vtkNew<vtkProperty> prop;
        prop->SetColor(1.0, 0.0, 0.0); // 红色高亮
        prop->SetLineWidth(2.0);
        prop->EdgeVisibilityOn();
        prop->SetEdgeColor(1.0, 0.0, 0.0);
        highlight_actor_->SetProperty(prop);
    }
}

FaceSelectorHighlight::~FaceSelectorHighlight()
{
    clear();
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
    _cancel_highlight(selected_mapper_);
    selections_.clear();
}

void FaceSelectorHighlight::select(double posx, double posy)
{
    vtkNew<vtkHardwarePicker> picker;
    picker->PickFromListOn();
    picker->AddPickList(&select_op_.getFaceActor());
    picker->Pick(posx, posy, 0, renderer_);

    vtkIdType pickedCellId = picker->GetCellId();
    if (pickedCellId == -1) {
        clear();
        return;
    }

    // 获取选中的 cell
    vtkActor* pickedActor = picker->GetActor();
    assert(pickedActor);
    vtkPolyDataMapper* pickedMapper = vtkPolyDataMapper::SafeDownCast(pickedActor->GetMapper());
    assert(pickedMapper);
    vtkPolyData* pickedPoly = pickedMapper->GetInput();
    assert(pickedPoly);
    
    // 检查是否已选中
    auto it = _find_selected(pickedCellId, selections_);

    if (it != selections_.end()) { // 已选中，取消选中
        selections_.erase(it);
    } else { // 未选中，添加
        selections_.push_back(pickedCellId);
    }

    vtkNew<vtkCellArray> cell_array;

    for (const auto& face : selections_) {
        cell_array->InsertNextCell(pickedPoly->GetCell(face));
    }

    vtkNew<vtkPolyData> highlight_poly;
    highlight_poly->SetPoints(pickedPoly->GetPoints()); // 使用原始数据的点集
    highlight_poly->SetPolys(cell_array); // 设置面单元

    selected_mapper_->SetInputData(highlight_poly); // 触发高亮演员更新渲染
}
