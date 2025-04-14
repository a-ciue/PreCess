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
#include <vtkNamedColors.h>
#include <vtkDataSetMapper.h>
#include <vtkPropPicker.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkPolyDataMapper.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkPolyDataMapper.h>
#include <vtkSelectionNode.h>
#include <vtkUnstructuredGrid.h>
#include <vtkExtractSelection.h>
#include <vtkLine.h>
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkPolyDataMapper.h>
#include <vtkPropAssembly.h>
#include <vtkAssembly.h>
#include <vtkAssemblyPath.h>
#include <vtkAssemblyNode.h>
#include <vtkAppendPolyData.h>
#include <vtkMapper.h>
#include "ModelUtil.h"
#include <vtkCompositeDataDisplayAttributes.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkCompositePolyDataMapper.h>


BlockSelectorHighlight::BlockSelectorHighlight(vtkMapper* selection_mapper)
{
	this->mapper_ = selection_mapper;
}

void BlockSelectorHighlight::clear() {
    // 取消高亮所有选中的actor
    for (auto& selection : selections_) {
        _cancel_highlight(selection);
    }
    selections_.clear();
}

std::vector<vtkIdType> BlockSelectorHighlight::get() {
    std::vector<vtkIdType> blocks;
    for (const auto& selection : selections_) {
        blocks.push_back(selection.block_id);
    }
    return blocks;
}

void BlockSelectorHighlight::select(double posx, double posy, vtkRenderer* renderer) {
    vtkNew<vtkCellPicker> picker;
    picker->AddPickList(this->collection->GetLastProp());
    picker->Pick(posx, posy,0,renderer);
    vtkProp* pickedProp = picker->GetViewProp();
    if (pickedProp) {
        std::cout << "Picked an object!" << std::endl;
        // 输出拾取的prop是哪个actor
        std::cout << "Picked Prop: " << pickedProp << std::endl;

        if (!pickedProp->IsA("vtkActor")) { return; }

        vtkActor* actor = vtkActor::SafeDownCast(pickedProp);
        vtkMapper* mapper = actor->GetMapper();

        if (!mapper->IsA("vtkCompositePolyDataMapper")) { return; }
        
        // if 点击到compositePolyDataMapper
        vtkCompositePolyDataMapper* cmapper = vtkCompositePolyDataMapper::SafeDownCast(mapper);
        std::optional<size_t> selected_index = _is_selected(picker->GetFlatBlockIndex(), selections_);
        if (selected_index) {
            _cancel_highlight(selections_[*selected_index]);
            selections_.erase(selections_.begin() + *selected_index);
        }
        else {
            //
            selections_.emplace_back(Actor{ new_actor, vtkSmartPointer<vtkProperty>::New() });
            selections_.back().backup_property->DeepCopy(new_actor->GetProperty());
            new_actor->GetProperty()->SetColor(1.0, 0.0, 0.0);
            renderer_->AddActor(new_actor);

        }


        if (_is_selected(picker->GetFlatBlockIndex(),this->selections_))
        {
            cmapper->SetBlockColor(picker->GetFlatBlockIndex(), 1, 1, 0);
        }
        else
        {
            
        }
        
    }
    else {
        std::cout << "No object picked!" << std::endl;
    }
}

void BlockSelectorHighlight::_cancel_highlight(Block& selection) {
    selection.actor->GetProperty()->SetColor(selection.backup_property->GetColor());
}

std::optional<size_t> BlockSelectorHighlight::_is_selected(const vtkIdType block_id, const std::vector<Block>& selections) {

    for (size_t i = 0; i < selections.size(); ++i) {
        if (selections[i].block_id == block_id) {
            // 如果找到,返回该 actor 的索引
            return i;
        }
    }
    return std::nullopt;
}