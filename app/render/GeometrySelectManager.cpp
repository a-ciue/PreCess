#include "GeometrySelectManager.h"
#include "GeometryActorManagerSelectOp.h"
#include "GeometryActorSelectOp.h"
#include "GeometrySelectorHighlight.h"
#include "Selection.h"

#include <IVtkTools_ShapePicker.hxx>
#include <NCollection_List.hxx>
#include <vtkCompositePolyDataMapper.h>
#include <vtkPartitionedDataSet.h>
#include <vtkRenderer.h>

#include <array>
#include <utility>
#include <vtkRenderer.h>

GeometrySelectManager::GeometrySelectManager(vtkRenderer& renderer, vtkActor& highlight_actor, GeometryActorManagerSelectOp& op)
    : op_(&op)
    , renderer_(&renderer)
    , highlight_actor_(&highlight_actor)
{
    highlight_data_ = vtkSmartPointer<vtkPartitionedDataSet>::New();
    highlight_mapper_ = vtkSmartPointer<vtkCompositePolyDataMapper>::New();
    highlight_mapper_->SetInputDataObject(highlight_data_);

    picker_ = vtkSmartPointer<IVtkTools_ShapePicker>::New();
    picker_->SetRenderer(renderer_);
    picker_->SetAreaSelection(false);

    op_->observeShapePicker(picker_.Get());
}

void GeometrySelectManager::select(double posx, double posy)
{
    if (this->select_mode_ == SelectMode::None)
        return;

    const int n = picker_->Pick(posx, posy, 0.0, renderer_);
    if (n <= 0)
        return;

    const auto& pickedShapes = picker_->GetPickedShapesIds(false);
    if (pickedShapes.IsEmpty())
        return;

    const IVtk_IdType shapeId = pickedShapes.First();
    auto component_id = op_->getComponentIdByShapeId(shapeId);
    if (!component_id)
        return;

    auto select_op = op_->getSelectOp(*component_id);
    if (!select_op)
        return;

    auto* sel = getOrCreateSelector(*component_id);
    if (!sel)
        return;

    if (select_mode_ == SelectMode::GeometrySolid) {
        GeomSolidId solidId = kInvalidGeomSolidId;
        std::vector<IVtk_IdType> faceSubIds;
        if (select_op->resolvePickedSolid(picker_.Get(), shapeId, solidId, faceSubIds))
            static_cast<GeometrySolidSelectorHighlight*>(sel)->toggleSolid(solidId, faceSubIds);
    } else {
        IVtk_IdType subId = -1;
        auto geomId = select_op->resolvePickedSubshape(picker_.Get(), shapeId, select_mode_, subId);
        if (!geomId)
            return;
        switch (select_mode_) {
        case SelectMode::GeometryFace:
            static_cast<GeometryFaceSelectorHighlight*>(sel)->toggle(subId, *geomId);
            break;
        case SelectMode::GeometryEdge:
            static_cast<GeometryEdgeSelectorHighlight*>(sel)->toggle(subId, *geomId);
            break;
        case SelectMode::GeometryVertex:
            static_cast<GeometryVertexSelectorHighlight*>(sel)->toggle(subId, *geomId);
            break;
        }
    }
}

void GeometrySelectManager::setSelectMode(SelectMode select_mode)
{
    if (select_mode == this->select_mode_)
        return;
    this->select_mode_ = select_mode;
    this->clearSelection();

    op_->setShapePickerMode(select_mode);

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

void GeometrySelectManager::setVertexSnapActive(bool on)
{
    op_->setShapePickerMode(on ? SelectMode::GeometryVertex : SelectMode::None);
}

std::optional<std::pair<Index, std::array<double, 3>>> GeometrySelectManager::snapGeometryVertex(double posx, double posy)
{
    if (picker_->Pick(posx, posy, 0.0, renderer_) <= 0) {
        return std::nullopt;
    }
    const auto& picked_shapes = picker_->GetPickedShapesIds(false);
    if (picked_shapes.IsEmpty()) {
        return std::nullopt;
    }

    const IVtk_IdType shape_id = picked_shapes.First();
    auto component_id = op_->getComponentIdByShapeId(shape_id);
    auto select_op = component_id ? op_->getSelectOp(*component_id) : std::nullopt;
    if (!select_op) {
        return std::nullopt;
    }

    IVtk_IdType sub_id = -1;
    auto geom_id = select_op->resolvePickedSubshape(picker_.Get(), shape_id, SelectMode::GeometryVertex, sub_id);
    if (!geom_id) {
        return std::nullopt;
    }
    auto point = select_op->vertexPoint(sub_id);
    if (!point) {
        return std::nullopt;
    }
    return std::pair<Index, std::array<double, 3>> { *geom_id, *point };
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
        sel = std::make_unique<GeometryFaceSelectorHighlight>(*highlight_data_, pid, std::move(*select_op));
        break;
    case SelectMode::GeometryEdge:
        sel = std::make_unique<GeometryEdgeSelectorHighlight>(*highlight_data_, pid, std::move(*select_op));
        break;
    case SelectMode::GeometryVertex:
        sel = std::make_unique<GeometryVertexSelectorHighlight>(*highlight_data_, pid, std::move(*select_op));
        break;
    case SelectMode::GeometrySolid:
        sel = std::make_unique<GeometrySolidSelectorHighlight>(*highlight_data_, pid, std::move(*select_op));
        break;
    default:
        return nullptr;
    }

    auto* ptr = sel.get();
    component_selectors_[component_id] = std::move(sel);
    return ptr;
}
