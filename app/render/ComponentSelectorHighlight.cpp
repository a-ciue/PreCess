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
    highlight_mapper_->SetRelativeCoincidentTopologyLineOffsetParameters(0, highlight::LINE_UNITS);
    highlight_mapper_->SetRelativeCoincidentTopologyPointOffsetParameter(highlight::POINT_UNITS);
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

void ComponentSelectorHighlight::updateHighlight()
{
    if (selected_components_.empty()) {
        highlight_data_->Initialize();
        highlight_actor_->SetVisibility(false);
        return;
    }

    highlight_data_->Initialize();

    for (Index component_id : selected_components_) {
        // 网格占三个分区，几何面体与几何点边分别占一个分区。
        static constexpr unsigned int k_partitions_per_component = 5;
        auto pid = [comp = static_cast<unsigned int>(component_id)](unsigned int off) {
            return comp * k_partitions_per_component + off;
        };
        if (auto select_op = mesh_op_.getSelectOp(component_id)) {
            if (!select_op->isVisible())
                continue;
            if (auto* poly_data = _get_poly_data(select_op->getSolidActor()))
                highlight_data_->SetPartition(pid(0), poly_data);
            if (auto* poly_data = _get_poly_data(select_op->getFaceActor()))
                highlight_data_->SetPartition(pid(1), poly_data);
            if (auto* poly_data = _get_poly_data(select_op->getEdgeActor()))
                highlight_data_->SetPartition(pid(2), poly_data);
        }

        if (auto select_op = geom_op_.getSelectOp(component_id)) {
            if (!select_op->isVisible())
                continue;
            if (auto* poly_data = _get_poly_data(select_op->getPolyActor()))
                highlight_data_->SetPartition(pid(3), poly_data);
            if (auto* line_data = _get_poly_data(select_op->getLineActor()))
                highlight_data_->SetPartition(pid(4), line_data);
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
    prop->SetLineWidth(3.0);
    prop->SetPointSize(8.0);
    prop->RenderLinesAsTubesOn();
    actor.SetProperty(prop);
}
