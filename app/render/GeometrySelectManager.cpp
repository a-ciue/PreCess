#include "GeometrySelectManager.h"
#include "GeometryActorManagerSelectOp.h"
#include "GeometryActorSelectOp.h"
#include "GeometrySelectorHighlight.h"
#include "Selection.h"
#include <vtkHardwarePicker.h>
#include <vtkRenderer.h>
#include <IVtkTools_ShapePicker.hxx>

GeometrySelectManager::GeometrySelectManager(GeometryActorManagerSelectOp& op)
    : op_(&op)
{
}

void GeometrySelectManager::bindRenderer(vtkRenderer* renderer, vtkActor* highlight_actor)
{
    this->renderer_ = renderer;
    this->highlight_actor_ = highlight_actor;
    if (!picker_) {
        picker_ = vtkSmartPointer<IVtkTools_ShapePicker>::New();
        picker_->SetRenderer(renderer);
        picker_->SetAreaSelection(false);
    }
}

void GeometrySelectManager::select(double posx, double posy)
{
    if (this->select_mode_ == SelectMode::None) return;
    if (this->select_mode_ < SelectMode::GeometryVertex) return;

    vtkNew<vtkHardwarePicker> idPicker;
    op_->addPropsToPickList(idPicker);
    idPicker->Pick(posx, posy, 0, renderer_);

    vtkActor* pickedActor = idPicker->GetActor();
    auto component_id = op_->getComponentId(pickedActor);
    if (!component_id) return;

    auto* sel = getOrCreateSelector(*component_id);
    if (sel) sel->select(posx, posy);
}

void GeometrySelectManager::setSelectMode(SelectMode select_mode)
{
    this->select_mode_ = select_mode;
    this->component_selectors_.clear();
}

void GeometrySelectManager::clearSelection()
{
    this->component_selectors_.clear();
}

std::unique_ptr<Selection> GeometrySelectManager::getSelection()
{
    auto result = std::make_unique<Selection>();

    ElementEnum::Type type;
    switch (select_mode_) {
    case SelectMode::GeometryVertex: type = ElementEnum::GeometryVertex; break;
    case SelectMode::GeometryFace:   type = ElementEnum::GeometryFace;   break;
    case SelectMode::GeometryEdge:   type = ElementEnum::GeometryEdge;   break;
    case SelectMode::GeometrySolid:  type = ElementEnum::GeometrySolid;  break;
    default: return nullptr;
    }

    for (auto& [comp_id, sel] : component_selectors_) {
        for (const auto& id : sel->get().ids)
            result->ids.push_back(id);
    }

    result->type = type;
    result->component_id = -1;
    return result;
}

GeometrySelectorHighlight* GeometrySelectManager::getOrCreateSelector(Index component_id)
{
    auto it = component_selectors_.find(component_id);
    if (it != component_selectors_.end())
        return it->second.get();

    auto select_op = op_->getSelectOp(component_id);
    if (!select_op) return nullptr;

    std::unique_ptr<GeometrySelectorHighlight> sel;
    switch (select_mode_) {
    case SelectMode::GeometryFace:
        sel = std::make_unique<GeometryFaceSelectorHighlight>(*renderer_, *highlight_actor_, std::move(*select_op), picker_);
        break;
    case SelectMode::GeometryEdge:
        sel = std::make_unique<GeometryEdgeSelectorHighlight>(*renderer_, *highlight_actor_, std::move(*select_op), picker_);
        break;
    case SelectMode::GeometryVertex:
        sel = std::make_unique<GeometryVertexSelectorHighlight>(*renderer_, *highlight_actor_, std::move(*select_op), picker_);
        break;
    case SelectMode::GeometrySolid:
        sel = std::make_unique<GeometrySolidSelectorHighlight>(*renderer_, *highlight_actor_, std::move(*select_op), picker_);
        break;
    default:
        return nullptr;
    }

    auto* ptr = sel.get();
    component_selectors_[component_id] = std::move(sel);
    return ptr;
}
