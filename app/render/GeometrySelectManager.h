#ifndef GEOMETRY_SELECT_MANAGER_H
#define GEOMETRY_SELECT_MANAGER_H
#include "Core.h"
#include "Selection.h"
#include "GeometrySelectorHighlight.h"
#include "GeometryActorSelectOp.h"

#include <memory>
#include <unordered_map>
#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtkActor.h>

class vtkRenderer;
class vtkHardwarePicker;
class vtkPartitionedDataSet;
class vtkCompositePolyDataMapper;
class GeometryActorManagerSelectOp;

class GeometrySelectManager {
public:
    GeometrySelectManager(vtkRenderer& renderer, vtkActor& highlight_actor, GeometryActorManagerSelectOp& op);

    void select(double posx, double posy);
    void setSelectMode(SelectMode select_mode);
    void clearSelection();
    std::unique_ptr<Selection> getSelection();

private:
    GeometrySelectorHighlight* getOrCreateSelector(Index component_id);

    GeometryActorManagerSelectOp* op_;
    SelectMode select_mode_ { SelectMode::None };
    vtkRenderer* renderer_;
    vtkSmartPointer<vtkHardwarePicker> component_picker_;
    vtkSmartPointer<IVtkTools_ShapePicker> picker_;

    vtkActor* highlight_actor_ {};
    vtkSmartPointer<vtkPartitionedDataSet> highlight_data_;
    vtkSmartPointer<vtkCompositePolyDataMapper> highlight_mapper_;

    std::unordered_map<Index, std::unique_ptr<GeometrySelectorHighlight>> component_selectors_;
};

#endif
