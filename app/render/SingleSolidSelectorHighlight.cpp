#include "SelectorHighlight.h"
#include "MeshActorSelectOp.h"

#include <vtkActor.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkDataSet.h>
#include <vtkDataSetMapper.h>
#include <vtkExtractSelection.h>
#include <vtkHardwarePicker.h>
#include <vtkIdTypeArray.h>
#include <vtkPropCollection.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSphereSource.h>

#include <spdlog/spdlog.h>

SingleSolidSelectorHighlight::SingleSolidSelectorHighlight(vtkRenderer* renderer)
    : renderer_(renderer)
{
    this->selected_ids_->SetName("vtkOriginalCellIds");
    this->selected_ids_->SetNumberOfComponents(1);
    this->selected_ids_->SetNumberOfValues(0);

    vtkNew<vtkDataSetMapper> mapper;
    this->highlight_actor_->SetMapper(mapper);
    this->highlight_actor_->GetProperty()->SetColor(1.0, 1.0, 0.0); // 黄色高亮
    this->highlight_actor_->GetProperty()->EdgeVisibilityOn();
    this->highlight_actor_->GetProperty()->SetEdgeColor(1.0, 0.0, 0.0); // 红色边框
    this->highlight_actor_->GetProperty()->SetLineWidth(2.0);
    
    renderer_->AddActor(this->highlight_actor_);
}
SingleSolidSelectorHighlight::~SingleSolidSelectorHighlight()
{
    clear();
    renderer_->RemoveActor(this->highlight_actor_);
}
SelectionVtk SingleSolidSelectorHighlight::get()
{
    SelectionVtk back_selection;
    back_selection.type = ElementEnum::Solid;
    if (selected_ids_->GetNumberOfValues()) {
        back_selection.ids.push_back(selected_ids_->GetValue(0));
    }
    return back_selection;
}

void SingleSolidSelectorHighlight::clear()
{
    _cancel_highlight(this->selected_ids_);
}

void SingleSolidSelectorHighlight::select(double posx, double posy)
{
    // 获取 picked_cell_id和picked_data_set
    vtkNew<vtkHardwarePicker> picker;
    picker->PickFromListOn();
    auto model_actor = model_actor_.lock();
    if (!model_actor) {
        clear();
        spdlog::debug("No solid actor set, selection cleared.");
        return;
    }
    picker->AddPickList(&model_actor->getSolidActor());
    picker->Pick(posx, posy, 0, renderer_);
    vtkIdType picked_cell_id = picker->GetCellId();
    if (picked_cell_id < 0) {
        clear();
        spdlog::debug("No cell picked, selection cleared.");
        return;
    }
    vtkDataSet* picked_data_set = picker->GetDataSet();

    // 获取对应的体id selected_solid_id
    auto solid_id_array = vtkIdTypeArray::SafeDownCast(picked_data_set->GetCellData()->GetArray("vtkOriginalCellIds"));
    if (!solid_id_array) {
        clear();
        spdlog::debug("Picked cell id: {}, no solid id array found.", picked_cell_id);
        return;
    }
    vtkIdType selected_solid_id = solid_id_array->GetValue(picked_cell_id);

    if (_is_selected(selected_solid_id, *this->selected_ids_)) {
        // 取消选中
        clear();
        spdlog::debug("Selected solid {} canceled.", selected_solid_id);
        return;
    } 

    this->selected_ids_->SetNumberOfValues(1);
    this->selected_ids_->SetValue(0, selected_solid_id);
    this->selected_ids_->Modified(); // 触发highlight_actor_更新
}

void SingleSolidSelectorHighlight::setCurModelActor(MeshActorSelectOpFactory model_actor)
{
    this->model_actor_ = model_actor;
    if (auto model_actor = this->model_actor_.lock()) {
        vtkSmartPointer extract_selection = model_actor->extractSolid(selected_ids_);
        this->highlight_actor_->GetMapper()->SetInputConnection(extract_selection->GetOutputPort());
    }
}

void SingleSolidSelectorHighlight::_cancel_highlight(vtkIdTypeArray* selected_ids)
{
    selected_ids->SetNumberOfValues(0);
    selected_ids->Modified();
}

bool SingleSolidSelectorHighlight::_is_selected(vtkIdType new_solid, const vtkIdTypeArray& selected_ids_)
{
    return selected_ids_.GetNumberOfValues() && new_solid == selected_ids_.GetValue(0);
}