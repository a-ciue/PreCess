#ifndef COMPONENT_SELECTOR_HIGHLIGHT_H
#define COMPONENT_SELECTOR_HIGHLIGHT_H

#include "Core.h"
#include "Selection.h"

#include <optional>
#include <vector>
#include <vtkSmartPointer.h>

class vtkRenderer;
class vtkActor;
class vtkMapper;
class vtkCompositePolyDataMapper;
class vtkPartitionedDataSet;
class vtkHardwarePicker;
class MeshActorManagerSelectOp;
class GeometryActorManagerSelectOp;

using SelectionVtk = Selection;

class ComponentSelectorHighlight {
public:
    static void setupHighlightStyle(vtkActor& actor, vtkMapper& mapper);

    ComponentSelectorHighlight(vtkRenderer& renderer,
        MeshActorManagerSelectOp& mesh_op,
        GeometryActorManagerSelectOp& geom_op);

    void select(double posx, double posy);
    void clear();
    SelectionVtk get() const;
    std::optional<Index> getLastPickedComponentId() const;

private:
    void updateHighlight();

    vtkRenderer* renderer_;
    MeshActorManagerSelectOp& mesh_op_;
    GeometryActorManagerSelectOp& geom_op_;

    std::vector<Index> selected_components_;
    std::optional<Index> last_picked_component_;

    vtkSmartPointer<vtkActor> highlight_actor_;
    vtkSmartPointer<vtkCompositePolyDataMapper> highlight_mapper_;
    vtkSmartPointer<vtkPartitionedDataSet> highlight_data_;
    vtkSmartPointer<vtkHardwarePicker> component_picker_;
};

#endif
