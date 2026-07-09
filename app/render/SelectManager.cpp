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
    if (select_mode == "Vertex") {
        mesh_->setSelectMode(SelectMode::Vertex);
        geom_->setSelectMode(SelectMode::None);
    } else if (select_mode == "Face") {
        mesh_->setSelectMode(SelectMode::Face);
        geom_->setSelectMode(SelectMode::None);
    } else if (select_mode == "Edge") {
        mesh_->setSelectMode(SelectMode::Edge);
        geom_->setSelectMode(SelectMode::None);
    } else if (select_mode == "Block") {
        mesh_->setSelectMode(SelectMode::Block);
        geom_->setSelectMode(SelectMode::None);
    } else if (select_mode == "Solid") {
        mesh_->setSelectMode(SelectMode::Solid);
        geom_->setSelectMode(SelectMode::None);
    } else if (select_mode == "GeometryFace") {
        mesh_->setSelectMode(SelectMode::None);
        geom_->setSelectMode(SelectMode::Face);
    } else if (select_mode == "GeometryEdge") {
        mesh_->setSelectMode(SelectMode::None);
        geom_->setSelectMode(SelectMode::Edge);
    } else if (select_mode == "GeometryVertex") {
        mesh_->setSelectMode(SelectMode::None);
        geom_->setSelectMode(SelectMode::Vertex);
    } else if (select_mode == "GeometrySolid") {
        mesh_->setSelectMode(SelectMode::None);
        geom_->setSelectMode(SelectMode::Solid);
    } else {
        mesh_->setSelectMode(SelectMode::None);
        geom_->setSelectMode(SelectMode::None);
    }
}

void SelectManager::clearSelection()
{
    mesh_->clearSelection();
    geom_->clearSelection();
}

std::unique_ptr<Selection> SelectManager::getSelection()
{
    auto geom_selection = geom_->getSelection();
    if (geom_selection) {
        return geom_selection;
    }
    return mesh_->getSelection();
}
