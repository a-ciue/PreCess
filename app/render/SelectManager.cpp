#include "SelectManager.h"
#include "GeometryActorManagerSelectOp.h"
#include "GeometrySelectManager.h"
#include "MeshActorManagerSelectOp.h"
#include "MeshSelectManager.h"

#include <vtkRenderer.h>

SelectManager::SelectManager(vtkRenderer& renderer,
    MeshActorManagerSelectOp& mesh_op, GeometryActorManagerSelectOp& geom_op)
{
    mesh_ = std::make_unique<MeshSelectManager>(renderer, *highlight_actor_, mesh_op);
    geom_ = std::make_unique<GeometrySelectManager>(renderer, *highlight_actor_, geom_op);
    highlight_actor_->PickableOff();
    highlight_actor_->SetVisibility(true);
    renderer.AddActor(highlight_actor_);
}

SelectManager::~SelectManager() = default;

void SelectManager::select(double posx, double posy)
{
    mesh_->select(posx, posy);
    geom_->select(posx, posy);
}

void SelectManager::setSelectMode(const std::string& select_mode)
{
    SelectMode mode = SelectMode::None;
    if (select_mode == "Vertex") {
        mode = SelectMode::Vertex;
    } else if (select_mode == "Face") {
        mode = SelectMode::Face;
    } else if (select_mode == "Edge") {
        mode = SelectMode::Edge;
    } else if (select_mode == "Block") {
        mode = SelectMode::Block;
    } else if (select_mode == "Solid") {
        mode = SelectMode::Solid;
    } else if (select_mode == "GeometryVertex") {
        mode = SelectMode::GeometryVertex;
    } else if (select_mode == "GeometryEdge") {
        mode = SelectMode::GeometryEdge;
    } else if (select_mode == "GeometryFace") {
        mode = SelectMode::GeometryFace;
    } else if (select_mode == "GeometrySolid") {
        mode = SelectMode::GeometrySolid;
    }

    mesh_->setSelectMode(mode);
    geom_->setSelectMode(mode);
}

void SelectManager::clearSelection()
{
    mesh_->clearSelection();
    geom_->clearSelection();
}

std::unique_ptr<Selection> SelectManager::getSelection()
{
    if (auto geom_selection = geom_->getSelection()) {
        return geom_selection;
    }
    return mesh_->getSelection();
}
