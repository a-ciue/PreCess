#include "SelectManager.h"
#include "ComponentSelectorHighlight.h"
#include "GeometryActorManagerSelectOp.h"
#include "GeometrySelectManager.h"
#include "MeshActorManagerSelectOp.h"
#include "MeshSelectManager.h"

#include <vtkRenderer.h>

SelectManager::SelectManager(vtkRenderer& renderer,
    MeshActorManagerSelectOp& mesh_op, GeometryActorManagerSelectOp& geom_op)
    : renderer_(&renderer)
{
    mesh_ = std::make_unique<MeshSelectManager>(renderer, *highlight_actor_, mesh_op);
    geom_ = std::make_unique<GeometrySelectManager>(renderer, *highlight_actor_, geom_op);
    component_selector_ = std::make_unique<ComponentSelectorHighlight>(renderer, mesh_op, geom_op);
    highlight_actor_->PickableOff();
    highlight_actor_->SetVisibility(true);
    renderer.AddActor(highlight_actor_);
}

SelectManager::~SelectManager() = default;

static bool is_mesh_mode(SelectMode m) { return m >= SelectMode::Vertex && m <= SelectMode::Solid; }
static bool is_geom_mode(SelectMode m) { return m >= SelectMode::GeometryVertex && m <= SelectMode::GeometrySolid; }

void SelectManager::select(double posx, double posy)
{
    if (select_mode_ == SelectMode::Component) {
        component_selector_->select(posx, posy);
        return;
    }
    if (is_mesh_mode(select_mode_))
        mesh_->select(posx, posy);
    else if (is_geom_mode(select_mode_))
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
    } else if (select_mode == "Component") {
        mode = SelectMode::Component;
    }

    select_mode_ = mode;

    if (mode == SelectMode::Component) {
        mesh_->clearSelection();
        geom_->clearSelection();
        return;
    }

    component_selector_->clear();
    if (is_mesh_mode(mode))
        mesh_->setSelectMode(mode);
    else
        mesh_->clearSelection();

    if (is_geom_mode(mode))
        geom_->setSelectMode(mode);
    else
        geom_->clearSelection();
}

void SelectManager::setFaceSelectionByAngle(bool enabled, double angle_deg)
{
    mesh_->setFaceSelectionByAngle(enabled, angle_deg);
}

void SelectManager::clearSelection()
{
    component_selector_->clear();
    mesh_->clearSelection();
    geom_->clearSelection();
}

void SelectManager::setMeshIdQuery(const IMeshIdQuery* id_query)
{
    mesh_->setMeshIdQuery(id_query);
}

void SelectManager::refreshComponentHighlight()
{
    component_selector_->refreshHighlight();
}

void SelectManager::setGeometryHighlightVisible(bool visible)
{
    geom_->setHighlightVisible(visible);
}

void SelectManager::setGeometryHighlightVisible(Index component_id, bool visible)
{
    geom_->setHighlightVisible(component_id, visible);
}

void SelectManager::setMeshHighlightVisible(bool visible)
{
    mesh_->setHighlightVisible(visible);
}

void SelectManager::setMeshHighlightVisible(Index component_id, bool visible)
{
    mesh_->setHighlightVisible(component_id, visible);
}

std::optional<std::pair<Index, std::array<double, 3>>> SelectManager::snapGeometryVertex(double posx, double posy)
{
    return geom_->snapGeometryVertex(posx, posy);
}

std::unique_ptr<Selection> SelectManager::getSelection()
{
    if (select_mode_ == SelectMode::Component) {
        auto result = std::make_unique<Selection>(component_selector_->get());
        result->component_id = -1;
        return result;
    }
    if (is_geom_mode(select_mode_))
        return geom_->getSelection();
    if (is_mesh_mode(select_mode_))
        return mesh_->getSelection();
    return nullptr;
}
