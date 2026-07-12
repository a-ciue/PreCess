#ifndef GEOMETRY_SELECT_MANAGER_H
#define GEOMETRY_SELECT_MANAGER_H
#include "Core.h"
#include "Selection.h"
#include "GeometrySelectorHighlight.h"
#include "GeometryActorSelectOp.h"

#include <memory>
#include <unordered_map>
#include <vtkNew.h>
#include <vtkActor.h>
#include <vtkPolyDataMapper.h>

class vtkRenderer;
class GeometryActorManagerSelectOp;

class GeometrySelectManager {
public:
    GeometrySelectManager(GeometryActorManagerSelectOp& op);

    void bindRenderer(vtkRenderer* renderer, vtkActor* highlight_actor);
    void select(double posx, double posy);
    void setSelectMode(SelectMode select_mode);
    void clearSelection();
    std::unique_ptr<Selection> getSelection();

private:
    GeometrySelectorHighlight* getOrCreateSelector(Index component_id);

    GeometryActorManagerSelectOp* op_;
    SelectMode select_mode_ { SelectMode::None };
    vtkRenderer* renderer_ {};
    vtkActor* highlight_actor_ {};
    vtkSmartPointer<IVtkTools_ShapePicker> picker_;
    std::unordered_map<Index, std::unique_ptr<GeometrySelectorHighlight>> component_selectors_;
};

#endif
