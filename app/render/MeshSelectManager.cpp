#include "MeshSelectManager.h"
#include "MeshActorManagerSelectOp.h"
#include "Selection.h"
#include "SelectorHighlight.h"
#include <vtkCompositePolyDataMapper.h>
#include <vtkHardwarePicker.h>
#include <vtkPartitionedDataSet.h>
#include <vtkRenderer.h>

MeshSelectManager::MeshSelectManager(vtkRenderer& renderer, vtkActor& highlight_actor, MeshActorManagerSelectOp& op)
    : op_(&op)
    , renderer_(&renderer)
    , highlight_actor_(&highlight_actor)
{
    component_picker_ = vtkSmartPointer<vtkHardwarePicker>::New();
    highlight_mapper_ = vtkSmartPointer<vtkCompositePolyDataMapper>::New();
    highlight_data_ = vtkSmartPointer<vtkPartitionedDataSet>::New();

    highlight_mapper_->SetInputDataObject(highlight_data_);
    component_picker_->PickFromListOn();
    op_->observePickList(component_picker_->GetPickList());
}

void MeshSelectManager::select(double posx, double posy)
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

void MeshSelectManager::setSelectMode(SelectMode select_mode)
{
    if (this->select_mode_ == select_mode)
        return;

    this->select_mode_ = select_mode;
    this->component_selectors_.clear();

    highlight_data_->Initialize();
    applyHighlightStyle(select_mode);
}

void MeshSelectManager::clearSelection()
{
    this->component_selectors_.clear();
}

void MeshSelectManager::setMeshIdQuery(const IMeshIdQuery* id_query)
{
    id_query_ = id_query;
}

std::unique_ptr<Selection> MeshSelectManager::getSelection()
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

SelectorHighlight* MeshSelectManager::getOrCreateSelector(Index component_id)
{
    auto it = component_selectors_.find(component_id);
    if (it != component_selectors_.end())
        return it->second.get();

    auto select_op = op_->getSelectOp(component_id);
    if (!select_op)
        return nullptr;

    unsigned int pid = component_selectors_.size();
    std::unique_ptr<SelectorHighlight> sel;
    switch (select_mode_) {
    case SelectMode::Face:
        sel = std::make_unique<FaceSelectorHighlight>(*renderer_, *highlight_data_, pid, std::move(*select_op));
        break;
    case SelectMode::Edge:
        sel = std::make_unique<EdgeSelectorHighlight>(*renderer_, *highlight_data_, pid, std::move(*select_op),
            component_id, id_query_);
        break;
    case SelectMode::Solid:
        sel = std::make_unique<SolidSelectorHighlight>(*renderer_, *highlight_data_, pid, std::move(*select_op));
        break;
    case SelectMode::Vertex:
        sel = std::make_unique<VertexSelectorHighlight>(*renderer_, *highlight_data_, pid, std::move(*select_op));
        break;
    case SelectMode::Block:
        sel = std::make_unique<BlockSelectorHighlight>(*renderer_, *highlight_actor_, std::move(*select_op));
        break;
    default:
        return nullptr;
    }

    auto* ptr = sel.get();
    component_selectors_[component_id] = std::move(sel);
    return ptr;
}

void MeshSelectManager::applyHighlightStyle(SelectMode mode)
{
    switch (mode) {
    case SelectMode::Face:
        FaceSelectorHighlight::setupHighlightStyle(*highlight_actor_, *highlight_mapper_);
        break;
    case SelectMode::Edge:
        EdgeSelectorHighlight::setupHighlightStyle(*highlight_actor_, *highlight_mapper_);
        break;
    case SelectMode::Solid:
        SolidSelectorHighlight::setupHighlightStyle(*highlight_actor_, *highlight_mapper_);
        break;
    case SelectMode::Vertex:
        VertexSelectorHighlight::setupHighlightStyle(*highlight_actor_, *highlight_mapper_);
        break;
    }
}
