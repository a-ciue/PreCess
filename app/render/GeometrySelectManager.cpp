#include "GeometrySelectManager.h"
#include "GeometryActor.h"
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <cassert>

void GeometrySelectManager::bindRenderer(vtkRenderer* renderer)
{
    this->renderer_ = renderer;
    if (!picker_) {
        picker_ = vtkSmartPointer<IVtkTools_ShapePicker>::New();
        picker_->SetRenderer(renderer);
        picker_->SetAreaSelection(false);
    }
}

void GeometrySelectManager::select(double posx, double posy)
{
    if (this->selector_) {
        this->selector_->select(posx, posy);
    }
}

void GeometrySelectManager::setSelectActor(std::weak_ptr<GeometryActor> geom_actor)
{
    this->cur_geom_actor_ = GeometryActorSelectOpFactory { geom_actor };
    if (this->selector_) {
        this->selector_->clear();
        this->selector_->setPicker(picker_);
        this->selector_->setCurGeomActor(*cur_geom_actor_);
    }
}

void GeometrySelectManager::setSelectMode(SelectMode select_mode)
{
    this->clearSelection();
    this->select_mode_ = select_mode;

    if (this->selector_) {
        this->selector_->setPicker(picker_);
        this->selector_->setCurGeomActor(GeometryActorSelectOpFactory {});
    }

    if (this->select_mode_ == SelectMode::GeometryVertex) {
        this->selector_ = std::make_unique<GeometryVertexSelectorHighlight>(this->renderer_);
    } else if (this->select_mode_ == SelectMode::GeometryFace) {
        this->selector_ = std::make_unique<GeometryFaceSelectorHighlight>(this->renderer_);
    } else if (this->select_mode_ == SelectMode::GeometryEdge) {
        this->selector_ = std::make_unique<GeometryEdgeSelectorHighlight>(this->renderer_);
    } else if (this->select_mode_ == SelectMode::GeometrySolid) {
        this->selector_ = std::make_unique<GeometrySolidSelectorHighlight>(this->renderer_);
    } else {
        this->selector_ = nullptr;
    }

    if (this->selector_ && this->cur_geom_actor_) {
        this->selector_->setPicker(picker_);
        this->selector_->setCurGeomActor(*cur_geom_actor_);
    }
}

void GeometrySelectManager::clearSelection()
{
    if (this->selector_) {
        this->selector_->clear();
    }
    if (this->renderer_ && this->renderer_->GetRenderWindow())
        this->renderer_->GetRenderWindow()->Render();
}

std::unique_ptr<Selection> GeometrySelectManager::getSelection()
{
    std::unique_ptr<Selection> selection = std::make_unique<Selection>();
    
    if (!this->selector_) 
    {
        return nullptr;
    }

    if (this->select_mode_ == SelectMode::GeometryVertex) {
        for (const auto& id : this->selector_->get().ids) {
            selection->ids.push_back(id);
        }
        selection->type = ElementEnum::GeometryVertex;
    } else if (this->select_mode_ == SelectMode::GeometryFace) {
        for (const auto& id : this->selector_->get().ids) {
            selection->ids.push_back(id);
        }
        selection->type = ElementEnum::GeometryFace;
    } else if (this->select_mode_ == SelectMode::GeometryEdge) {
        for (const auto& id : this->selector_->get().ids) {
            selection->ids.push_back(id);
        }
        selection->type = ElementEnum::GeometryEdge;
    } else if (this->select_mode_ == SelectMode::GeometrySolid) {
        for (const auto& id : this->selector_->get().ids) {
            selection->ids.push_back(id);
        }
        selection->type = ElementEnum::GeometrySolid;
    } else {
        if (this->selector_)
            this->selector_->clear();
        return nullptr;
    }

    return selection;
}
