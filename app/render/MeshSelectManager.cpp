#include "MeshSelectManager.h"
#include "MeshActor.h"
#include "Selection.h"
#include "SelectorHighlight.h"
#include <vtkActor.h>
#include <vtkAppendPolyData.h>
#include <vtkCellArray.h>
#include <vtkProperty.h>

void MeshSelectManager::bindRenderer(vtkRenderer* renderer, vtkActor* highlight_actor)
{
    this->renderer_ = renderer;
    this->highlight_actor_ = highlight_actor;
}

void MeshSelectManager::select(double posx, double posy)
{
    if (this->selector_) {
        assert(this->select_mode_ != SelectMode::None);
        this->selector_->select(posx, posy);
    }
}

void MeshSelectManager::setSelectActor(std::weak_ptr<MeshActor> model_actor_)
{
    this->cur_model_actor_ = MeshActorSelectOpFactory { model_actor_ };
    if (this->selector_) {
        this->selector_->clear();
        this->selector_->setCurModelActor(*cur_model_actor_);
    }
}

void MeshSelectManager::setSelectMode(SelectMode select_mode)
{
    this->select_mode_ = select_mode;
    if (this->select_mode_ == SelectMode::Vertex) {
        this->selector_ = std::make_unique<VertexSelectorHighlight>(this->renderer_, this->highlight_actor_);
    } else if (this->select_mode_ == SelectMode::Face) {
        this->selector_ = std::make_unique<FaceSelectorHighlight>(this->renderer_, this->highlight_actor_);
    } else if (this->select_mode_ == SelectMode::Block) {
        this->selector_ = std::make_unique<BlockSelectorHighlight>(this->renderer_);
    } else if (this->select_mode_ == SelectMode::Edge) {
        this->selector_ = std::make_unique<EdgeSelectorHighlight>(this->renderer_, this->highlight_actor_);
    } else if (this->select_mode_ == SelectMode::Solid) {
        this->selector_ = std::make_unique<SolidSelectorHighlight>(this->renderer_, this->highlight_actor_);
    } else {
        this->selector_ = nullptr;
    }

    if (this->selector_ && this->cur_model_actor_) {
        this->selector_->setCurModelActor(*cur_model_actor_);
    }
}

void MeshSelectManager::clearSelection()
{
    if (this->selector_) {
        this->selector_->clear();
    }
}

std::unique_ptr<Selection> MeshSelectManager::getSelection()
{
    std::unique_ptr<Selection> selection = std::make_unique<Selection>();

    if (this->select_mode_ == SelectMode::Vertex) {
        for (const auto& id : this->selector_->get().ids) {
            selection->ids.push_back(id);
        }
        selection->type = ElementEnum::Vertex;
    } else if (this->select_mode_ == SelectMode::Face) {
        for (const auto& id : this->selector_->get().ids) {
            selection->ids.push_back(id);
        }
        selection->type = ElementEnum::Face;
    } else if (this->select_mode_ == SelectMode::Block) {
        for (const auto& id : this->selector_->get().ids) {
            selection->ids.push_back(id);
        }
        selection->type = ElementEnum::Block;
    }else if (this->select_mode_ == SelectMode::Solid) {
        for (const auto& id : this->selector_->get().ids) {
            selection->ids.push_back(id);
        }
        selection->type = ElementEnum::Solid;
    } else if (this->select_mode_ == SelectMode::Edge) {
        for (const auto& id : this->selector_->get().ids) {
            selection->ids.push_back(id);
        }
        selection->type = ElementEnum::Edge;
    } else {
        if (this->selector_)
            this->selector_->clear();
        return nullptr;
    }

    return selection;
}
