#include "MeshActor.h"
#include "MeshActorSelectOp.h"
#include "Selection.h"
#include "SelectorHighlight.h"
#include <spdlog/spdlog.h>
#include <vtkDataSetMapper.h>
#include <vtkHardwarePicker.h>
#include <vtkPolyData.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

FaceSelectorHighlight::FaceSelectorHighlight(vtkRenderer* renderer)
{
    this->selected_actor_ = vtkSmartPointer<vtkActor>::New();
    this->renderer_ = renderer;

    selected_mapper_->SetInputData(vtkPolyData::New());
    selected_actor_->SetMapper(selected_mapper_);
    selected_actor_->GetProperty()->SetColor(1.0, 0.0, 0.0); // 红色高亮
    selected_actor_->GetProperty()->SetLineWidth(2.0);
    selected_actor_->GetProperty()->EdgeVisibilityOn();
    selected_actor_->GetProperty()->SetEdgeColor(1.0, 0.0, 0.0);
    selected_actor_->PickableOff(); // 防止自己被选中

    renderer_->AddActor(selected_actor_);
}

FaceSelectorHighlight::~FaceSelectorHighlight()
{
    renderer_->RemoveActor(selected_actor_);
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
    collection_->InitTraversal();
    for (vtkProp* actor {}; actor = collection_->GetNextProp();) {
        picker->AddPickList(actor);
    }
    picker->Pick(posx, posy, 0, renderer_);

    vtkIdType pickedCellId = picker->GetCellId();
    if (pickedCellId != -1) {
        // 获取选中的 cell
        vtkActor* pickedActor = picker->GetActor();
        if (!pickedActor)
            return;
        vtkPolyDataMapper* pickedMapper = vtkPolyDataMapper::SafeDownCast(pickedActor->GetMapper());
        if (!pickedMapper) {
            return;
        }
        vtkPolyData* pickedPoly = pickedMapper->GetInput();
        if (!pickedPoly)
            return; // 数据无效则返回

        // 检查是否已选中
        auto it = _find_selected(pickedCellId, selections_);

        if (it != selections_.end()) {
            // 已选中，取消选中
            selections_.erase(it);
        } else {
            // 未选中，添加
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
    } else {
        // 没选到
        clear();
    }
}

void FaceSelectorHighlight::setCurModelActor(MeshActorSelectOpFactory model_actor)
{
    this->collection_->RemoveAllItems();
    if (auto actor = model_actor.lock()) {
        this->collection_->AddItem(&actor->getFaceActor());
    }
    this->model_actor_ = model_actor;
}

void FaceSelectorHighlight::_cancel_highlight(vtkDataSetMapper* selectedMapper)
{
    vtkNew<vtkPolyData> empty;
    selectedMapper->SetInputData(empty);
}

std::vector<vtkIdType>::const_iterator FaceSelectorHighlight::_find_selected(vtkIdType new_face_id, const std::vector<vtkIdType>& selections)
{
    return std::find_if(selections.begin(), selections.end(),
        [&](const vtkIdType& id) {
            return id == new_face_id;
        });
}
