#include "SelectManager.h"
#include "GeometrySelectManager.h"
#include "MeshSelectManager.h"
#include "MeshActorManagerSelectOp.h"
#include "GeometryActorManagerSelectOp.h"

#include <vtkMapper.h>
#include <vtkPolyData.h>
#include <vtkRenderer.h>

SelectManager::SelectManager()
{
    vtkNew<vtkPolyData> empty;
    empty_mapper_->SetInputData(empty);
    highlight_actor_->SetMapper(empty_mapper_);
    highlight_actor_->PickableOff();
    highlight_actor_->SetVisibility(true);
}

SelectManager::~SelectManager() = default;

void SelectManager::bindRenderer(vtkRenderer* renderer)
{
    if (renderer)
        renderer->AddActor(highlight_actor_);
    mesh_->bindRenderer(renderer, highlight_actor_);
    geom_->bindRenderer(renderer, highlight_actor_);
}

void SelectManager::setOps(MeshActorManagerSelectOp& mesh_op, GeometryActorManagerSelectOp& geom_op)
{
    mesh_ = std::make_unique<MeshSelectManager>(mesh_op);
    geom_ = std::make_unique<GeometrySelectManager>(geom_op);
}

void SelectManager::select(double posx, double posy)
{
    mesh_->select(posx, posy);
    geom_->select(posx, posy);
}

void SelectManager::setSelectMode(const std::string& select_mode)
{
    highlight_actor_->SetMapper(empty_mapper_);

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
