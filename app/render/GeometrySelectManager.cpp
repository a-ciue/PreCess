#include "GeometrySelectManager.h"
#include "GeometryActorManagerSelectOp.h"
#include "GeometryActorSelectOp.h"
#include "GeometrySelectorHighlight.h"
#include "Selection.h"
#include <IVtkTools_ShapePicker.hxx>
#include <vtkCompositePolyDataMapper.h>
#include <vtkHardwarePicker.h>
#include <vtkPartitionedDataSet.h>
#include <vtkRenderer.h>

GeometrySelectManager::GeometrySelectManager(vtkRenderer& renderer, vtkActor& highlight_actor, GeometryActorManagerSelectOp& op)
    : op_(&op)
    , renderer_(&renderer)
    , highlight_actor_(&highlight_actor)
{
    highlight_data_ = vtkSmartPointer<vtkPartitionedDataSet>::New();
    highlight_mapper_ = vtkSmartPointer<vtkCompositePolyDataMapper>::New();
    highlight_mapper_->SetInputDataObject(highlight_data_);

    component_picker_ = vtkSmartPointer<vtkHardwarePicker>::New();
    component_picker_->PickFromListOn();
    op_->observePickList(component_picker_->GetPickList());

    picker_ = vtkSmartPointer<IVtkTools_ShapePicker>::New();
    picker_->SetRenderer(renderer_);
    picker_->SetAreaSelection(false);
}

void GeometrySelectManager::select(double posx, double posy)
{
    if (this->select_mode_ == SelectMode::None)
        return;

    component_picker_->Pick(posx, posy, 0, renderer_);

    vtkActor* picked_actor = component_picker_->GetActor();
    auto component_id = op_->getComponentId(picked_actor);
    if (!component_id)
        return;

    if (auto* sel = getOrCreateSelector(*component_id))
        sel->select(posx, posy);
}

void GeometrySelectManager::setSelectMode(SelectMode select_mode)
{
    if (select_mode == this->select_mode_)
        return;
    this->select_mode_ = select_mode;
    this->clearSelection();

    switch (select_mode) {
    case SelectMode::GeometryFace:
        GeometryFaceSelectorHighlight::setupHighlightStyle(*highlight_actor_, *highlight_mapper_);
        break;
    case SelectMode::GeometryEdge:
        GeometryEdgeSelectorHighlight::setupHighlightStyle(*highlight_actor_, *highlight_mapper_);
        break;
    case SelectMode::GeometryVertex:
        GeometryVertexSelectorHighlight::setupHighlightStyle(*highlight_actor_, *highlight_mapper_);
        break;
    case SelectMode::GeometrySolid:
        GeometrySolidSelectorHighlight::setupHighlightStyle(*highlight_actor_, *highlight_mapper_);
        break;
    }
}

void GeometrySelectManager::clearSelection()
{
    this->component_selectors_.clear();
    highlight_data_->Initialize();
}

std::unique_ptr<Selection> GeometrySelectManager::getSelection()
{
    auto result = std::make_unique<Selection>();

    for (auto& [comp_id, sel] : component_selectors_) {
        auto selection = sel->get();
        for (const auto& id : selection.ids)
            result->ids.push_back(id);
        if (selection.ids.size()) {
            result->component_id = comp_id;
            result->type = selection.type;
        }
    }

    return result;
}

GeometrySelectorHighlight* GeometrySelectManager::getOrCreateSelector(Index component_id)
{
    auto it = component_selectors_.find(component_id);
    if (it != component_selectors_.end())
        return it->second.get();

    auto select_op = op_->getSelectOp(component_id);
    if (!select_op)
        return nullptr;

    unsigned int pid = component_selectors_.size();
    std::unique_ptr<GeometrySelectorHighlight> sel;
    switch (select_mode_) {
    case SelectMode::GeometryFace:
        sel = std::make_unique<GeometryFaceSelectorHighlight>(*renderer_, *highlight_data_, pid, std::move(*select_op), picker_);
        break;
    case SelectMode::GeometryEdge:
        sel = std::make_unique<GeometryEdgeSelectorHighlight>(*renderer_, *highlight_data_, pid, std::move(*select_op), picker_);
        break;
    case SelectMode::GeometryVertex:
        sel = std::make_unique<GeometryVertexSelectorHighlight>(*renderer_, *highlight_data_, pid, std::move(*select_op), picker_);
        break;
    case SelectMode::GeometrySolid:
        sel = std::make_unique<GeometrySolidSelectorHighlight>(*renderer_, *highlight_data_, pid, std::move(*select_op), picker_);
        break;
    default:
        return nullptr;
    }

    auto* ptr = sel.get();
    component_selectors_[component_id] = std::move(sel);
    return ptr;
}
