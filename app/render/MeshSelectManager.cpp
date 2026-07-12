#include "MeshSelectManager.h"
#include "MeshActorManagerSelectOp.h"
#include "Selection.h"
#include "SelectorHighlight.h"
#include <vtkHardwarePicker.h>
#include <vtkRenderer.h>

MeshSelectManager::MeshSelectManager(MeshActorManagerSelectOp& op)
    : op_(&op)
{
}

void MeshSelectManager::bindRenderer(vtkRenderer* renderer, vtkActor* highlight_actor)
{
    this->renderer_ = renderer;
    this->highlight_actor_ = highlight_actor;
}

void MeshSelectManager::select(double posx, double posy)
{
    if (this->select_mode_ == SelectMode::None)
        return;

    vtkNew<vtkHardwarePicker> idPicker;
    op_->addPropsToPickList(idPicker);
    idPicker->Pick(posx, posy, 0, renderer_);

    vtkActor* pickedActor = idPicker->GetActor();
    auto component_id = op_->getComponentId(pickedActor);
    if (!component_id)
        return;

    auto* sel = getOrCreateSelector(*component_id);
    if (sel)
        sel->select(posx, posy);
}

void MeshSelectManager::setSelectMode(SelectMode select_mode)
{
    this->select_mode_ = select_mode;
    this->component_selectors_.clear();
}

void MeshSelectManager::clearSelection()
{
    this->component_selectors_.clear();
}

std::unique_ptr<Selection> MeshSelectManager::getSelection()
{
    auto result = std::make_unique<Selection>();

    ElementEnum::Type type;
    switch (select_mode_) {
    case SelectMode::Vertex:
        type = ElementEnum::Vertex;
        break;
    case SelectMode::Face:
        type = ElementEnum::Face;
        break;
    case SelectMode::Edge:
        type = ElementEnum::Edge;
        break;
    case SelectMode::Solid:
        type = ElementEnum::Solid;
        break;
    case SelectMode::Block:
        type = ElementEnum::Block;
        break;
    default:
        return nullptr;
    }

    for (auto& [comp_id, sel] : component_selectors_) {
        for (const auto& id : sel->get().ids)
            result->ids.push_back(id);
    }

    result->type = type;
    result->component_id = -1;
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

    std::unique_ptr<SelectorHighlight> sel;
    switch (select_mode_) {
    case SelectMode::Face:
        sel = std::make_unique<FaceSelectorHighlight>(*renderer_, *highlight_actor_, std::move(*select_op));
        break;
    case SelectMode::Edge:
        sel = std::make_unique<EdgeSelectorHighlight>(*renderer_, *highlight_actor_, std::move(*select_op));
        break;
    case SelectMode::Solid:
        sel = std::make_unique<SolidSelectorHighlight>(*renderer_, *highlight_actor_, std::move(*select_op));
        break;
    case SelectMode::Vertex:
        sel = std::make_unique<VertexSelectorHighlight>(*renderer_, *highlight_actor_, std::move(*select_op));
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
