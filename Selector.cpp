#include "Selector.h"
#include <array>
#include <optional>
#include <utility>
#include <vector>
#include <vtkActor.h>
#include <vtkCellData.h>
#include <vtkCellPicker.h>
#include <vtkMapper.h>
#include <vtkNew.h>
#include <vtkDataSetMapper.h>
#include <vtkPropPicker.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>


namespace Selector {
    std::optional<std::pair<vtkActor*, int>> pick_cell(double posx, double posy, vtkRenderer* renderer) {
        //vtkNew<vtkPropPicker> actorpicker;
        vtkNew<vtkCellPicker> cellpicker;
        cellpicker->Pick(posx, posy, 0, renderer);


        vtkActor* actor = cellpicker->GetActor();
        if (!actor) {

            return std::nullopt;
        }
        vtkNew<vtkCellPicker> cellpicker;

        vtkIdType cellId = cellpicker->GetCellId();

        // 如果cellId为-1，表示没有拾取到单元格
        if (cellId == -1) {
            return std::nullopt;
        }

        // 在Actor中的位置
        int localCellIndex = cellpicker->GetCellId();


        // 返回actor和单元格的索引
        return std::make_pair(actor, localCellIndex);
    }
}

ActorSelectorHighlight::ActorSelectorHighlight(vtkRenderer* renderer)
{
    renderer_ = renderer;

}

void ActorSelectorHighlight::clear() {
    // 取消高亮所有选中的actor
    for (auto& selection : selections_) {
        _cancel_highlight(selection);
    }
    selections_.clear();
}

std::vector<vtkActor*> ActorSelectorHighlight::get() {
    // 返回当前选中的actors
    std::vector<vtkActor*> actors;
    for (const auto& selection : selections_) {
        actors.push_back(selection.actor);
    }
    return actors;
}

void ActorSelectorHighlight::select(double posx, double posy) {
    // 找到actor，高亮
    auto result = Selector::pick_cell(posx, posy, renderer_);
    if (result) {
        vtkActor* new_actor = result->first;
        int local_cell_index = result->second;

       
        std::optional<size_t> selected_index = _is_selected(new_actor, selections_);
        if (selected_index) {
            _cancel_highlight(selections_[*selected_index]);
            selections_.erase(selections_.begin() + *selected_index);
        }
        else {
            // 
            selections_.emplace_back(Actor{ new_actor, vtkSmartPointer<vtkProperty>::New() });
            selections_.back().backup_property->DeepCopy(new_actor->GetProperty());
            new_actor->GetProperty()->SetColor(1.0, 0.0, 0.0);
        }
    }
}

void ActorSelectorHighlight::_cancel_highlight(Actor& selection) {
    // 取消高亮，修改回原来属性
    selection.actor->SetProperty(selection.backup_property);
}

std::optional<size_t> ActorSelectorHighlight::_is_selected(const vtkActor* new_actor, const std::vector<Actor>& selections) {
    
    for (size_t i = 0; i < selections.size(); ++i) {
        if (selections[i].actor == new_actor) {
            // 如果找到,返回该 actor 的索引
            return i;
        }
    }
    return std::nullopt;
}

SingleFaceSelectorHighlight::SingleFaceSelectorHighlight(vtkRenderer* renderer)
    : renderer_(renderer) {
    
    renderer_->AddActor(selectedActor_);
}

SingleFaceSelectorHighlight::~SingleFaceSelectorHighlight() {
    
    renderer_->RemoveActor(selectedActor_);
}

std::optional<SingleFaceSelectorHighlight::SelectedFace> SingleFaceSelectorHighlight::get() {
    
    return selection_;
}

void SingleFaceSelectorHighlight::clear() {
    // 清空selection取消高亮
    _cancel_highlight(selectedMapper_);
    selection_ = std::nullopt;
}

void SingleFaceSelectorHighlight::select(double posx, double posy) {
    // 找到face并存储
    auto result = Selector::pick_cell(posx, posy, renderer_);
    if (result) {
        vtkActor* new_actor = result->first;
        int local_id = result->second;
        SelectedFace new_face = { new_actor, local_id };

        if (_is_selected(new_face, selection_)) {
           
            _cancel_highlight(selectedMapper_);
            selection_ = std::nullopt;
        }
        else {
            // 选中新的face
            selection_ = new_face;
            selectedMapper_->SetInputData(new_actor->GetMapper()->GetInput());
            selectedActor_->SetMapper(selectedMapper_);
            selectedActor_->GetProperty()->SetColor(1.0, 0.0, 0.0);
            
        }
    }
    else {
        
        _cancel_highlight(selectedMapper_);
        selection_ = std::nullopt;
    }
}

void SingleFaceSelectorHighlight::_cancel_highlight(vtkDataSetMapper* selectedMapper) {
    // 取消高亮,清空mapper
    selectedMapper->SetInputData(nullptr);
}

bool SingleFaceSelectorHighlight::_is_selected(SelectedFace new_face, const std::optional<SelectedFace>& selection) {
   
    if (selection && selection->actor == new_face.actor && selection->local_id == new_face.local_id) {
        return true;
    }
    return false;
}