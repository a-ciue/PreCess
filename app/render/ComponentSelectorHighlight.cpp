#include "ComponentSelectorHighlight.h"
#include "CoincidentTopology.h"
#include "GeometryActorManagerSelectOp.h"
#include "GeometryActorSelectOp.h"
#include "MeshActorManagerSelectOp.h"

#include <algorithm>
#include <vtkActor.h>
#include <vtkCompositePolyDataMapper.h>
#include <vtkHardwarePicker.h>
#include <vtkMapper.h>
#include <vtkPartitionedDataSet.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

#include "MeshAreaPick.h"

#include <set>
#include <vtkDataObject.h>

namespace {

std::vector<Index>::const_iterator _find_component(Index component_id, const std::vector<Index>& selections)
{
    return std::find(selections.begin(), selections.end(), component_id);
}

vtkPolyData* _get_poly_data(vtkProp& prop)
{
    auto* actor = vtkActor::SafeDownCast(&prop);
    if (!actor)
        return nullptr;
    auto* mapper = vtkPolyDataMapper::SafeDownCast(actor->GetMapper());
    if (!mapper)
        return nullptr;
    mapper->Update();
    return vtkPolyData::SafeDownCast(mapper->GetInput());
}

}

ComponentSelectorHighlight::ComponentSelectorHighlight(vtkRenderer& renderer,
    MeshActorManagerSelectOp& mesh_op, GeometryActorManagerSelectOp& geom_op)
    : renderer_(&renderer)
    , mesh_op_(mesh_op)
    , geom_op_(geom_op)
{
    highlight_actor_ = vtkSmartPointer<vtkActor>::New();
    highlight_mapper_ = vtkSmartPointer<vtkCompositePolyDataMapper>::New();
    highlight_mapper_->SetRelativeCoincidentTopologyPolygonOffsetParameters(0, highlight::POLYGON_UNITS);
    highlight_data_ = vtkSmartPointer<vtkPartitionedDataSet>::New();
    highlight_mapper_->SetInputDataObject(highlight_data_);

    setupHighlightStyle(*highlight_actor_, *highlight_mapper_);
    highlight_actor_->PickableOff();

    renderer.AddActor(highlight_actor_);
    highlight_actor_->SetVisibility(false);

    component_picker_ = vtkSmartPointer<vtkHardwarePicker>::New();
    component_picker_->PickFromListOn();
    mesh_op_.observePickList(component_picker_->GetPickList());
    geom_op_.observePickList(component_picker_->GetPickList());
}

void ComponentSelectorHighlight::clear()
{
    selected_components_.clear();
    highlight_data_->Initialize();
    highlight_actor_->SetVisibility(false);
}

void ComponentSelectorHighlight::refreshHighlight()
{
    updateHighlight();
}

SelectionVtk ComponentSelectorHighlight::get() const
{
    SelectionVtk back_selection;
    back_selection.type = ElementEnum::Component;
    for (const auto& component_id : selected_components_)
        back_selection.ids.push_back(component_id);
    return back_selection;
}

void ComponentSelectorHighlight::select(double posx, double posy)
{
    component_picker_->Pick(posx, posy, 0, renderer_);

    vtkActor* picked_actor = component_picker_->GetActor();
    if (!picked_actor) {
        clear();
        return;
    }

    auto component_id = mesh_op_.getComponentId(picked_actor);
    if (!component_id)
        component_id = geom_op_.getComponentId(picked_actor);
    if (!component_id) {
        return;
    }

    auto it = _find_component(*component_id, selected_components_);
    if (it != selected_components_.end())
        selected_components_.erase(it);
    else
        selected_components_.push_back(*component_id);

    updateHighlight();
}

void ComponentSelectorHighlight::selectArea(int xmin, int ymin, int xmax, int ymax)
{
    // 框选恒为替换：先清空，再选中本次框内的全部组件
    clear();

    std::vector<vtkActor*> targets;
    targets.reserve(64);
    for (Index comp_id : mesh_op_.getAllComponentIds()) {
        auto select_op = mesh_op_.getSelectOp(comp_id);
        if (!select_op || !select_op->isVisible())
            continue;
        for (vtkProp* p : { &select_op->getSolidActor(), &select_op->getFaceActor(), &select_op->getEdgeActor() })
            if (auto* actor = vtkActor::SafeDownCast(p))
                targets.push_back(actor);
    }
    for (Index comp_id : geom_op_.getAllComponentIds()) {
        auto select_op = geom_op_.getSelectOp(comp_id);
        if (!select_op || !select_op->isVisible())
            continue;
        if (auto* actor = vtkActor::SafeDownCast(&select_op->getPolyActor()))
            targets.push_back(actor);
    }

    if (targets.empty())
        return;

    auto hits = area_pick::executeAreaPicks(renderer_, targets,
        xmin, ymin, xmax, ymax, vtkDataObject::FIELD_ASSOCIATION_CELLS);

    std::set<Index> hit_components;
    for (const auto& [prop, ids] : hits) {
        auto component_id = mesh_op_.getComponentId(prop);
        if (!component_id)
            component_id = geom_op_.getComponentId(prop);
        if (component_id)
            hit_components.insert(*component_id);
    }

    for (Index component_id : hit_components)
        selected_components_.push_back(component_id);

    updateHighlight();
}

void ComponentSelectorHighlight::updateHighlight()
{
    if (selected_components_.empty()) {
        highlight_data_->Initialize();
        highlight_actor_->SetVisibility(false);
        return;
    }

    highlight_data_->Initialize();

    for (Index component_id : selected_components_) {
        static constexpr unsigned int k_partitions_per_component = 4;
        auto pid = [comp = static_cast<unsigned int>(component_id)](unsigned int off) {
            return comp * k_partitions_per_component + off;
        };
        if (auto select_op = mesh_op_.getSelectOp(component_id);
            select_op && select_op->isVisible()) {
            if (auto* poly_data = _get_poly_data(select_op->getSolidActor()))
                highlight_data_->SetPartition(pid(0), poly_data);
            if (auto* poly_data = _get_poly_data(select_op->getFaceActor()))
                highlight_data_->SetPartition(pid(1), poly_data);
            if (auto* poly_data = _get_poly_data(select_op->getEdgeActor()))
                highlight_data_->SetPartition(pid(2), poly_data);
        }

        if (auto select_op = geom_op_.getSelectOp(component_id);
            select_op && select_op->isVisible()) {
            if (auto* poly_data = _get_poly_data(select_op->getPolyActor()))
                highlight_data_->SetPartition(pid(3), poly_data);
        }
    }

    highlight_data_->Modified();
    highlight_actor_->SetVisibility(true);
}

void ComponentSelectorHighlight::setupHighlightStyle(vtkActor& actor, vtkMapper& mapper)
{
    actor.SetMapper(&mapper);
    vtkNew<vtkProperty> prop;
    prop->SetColor(1.0, 0.0, 0.0);
    prop->SetOpacity(0.3);
    actor.SetProperty(prop);
}
