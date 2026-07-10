#include "SelectManager.h"
#include "MeshSelectManager.h"
#include "GeometrySelectManager.h"
#include "GeometryActor.h"
#include "MeshActor.h"

SelectManager::SelectManager()
    : mesh_(std::make_unique<MeshSelectManager>())
    , geom_(std::make_unique<GeometrySelectManager>())
{
}

SelectManager::~SelectManager() = default;

void SelectManager::bindRenderer(vtkRenderer* renderer)
{
    mesh_->bindRenderer(renderer);
    geom_->bindRenderer(renderer);
}

void SelectManager::select(double posx, double posy)
{
    mesh_->select(posx, posy);
    geom_->select(posx, posy);
}

void SelectManager::setSelectActor(std::weak_ptr<MeshActor> mesh_actor)
{
    mesh_->setSelectActor(mesh_actor);
}

void SelectManager::setSelectActor(std::weak_ptr<GeometryActor> geom_actor)
{
    geom_->setSelectActor(geom_actor);
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
