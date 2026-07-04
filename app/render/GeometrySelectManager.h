#ifndef GEOMETRY_SELECT_MANAGER_H
#define GEOMETRY_SELECT_MANAGER_H
#include "GeometrySelectorHighlight.h"
#include "GeometryActorSelectOp.h"
#include "Core.h"

#include <memory>
#include <optional>
#include <vtkNew.h>
#include <vtkActor.h>
#include <vtkPolyDataMapper.h>

class vtkRenderer;
struct GeometrySubshapeIndex;

class GeometrySelectManager {
public:
    void bindRenderer(vtkRenderer* renderer);
    void select(double posx, double posy);

    void setSelectActor(std::weak_ptr<GeometryActor> geom_actor);
    void setSelectMode(SelectMode select_mode);
    void clearSelection();
    std::unique_ptr<Selection> getSelection();

private:
    std::optional<GeometryActorSelectOpFactory> cur_geom_actor_ {};
    SelectMode select_mode_ { SelectMode::None };
    vtkNew<vtkActor> selection_actor_;
    vtkNew<vtkPolyDataMapper> selection_mapper_;
    vtkSmartPointer<IVtkTools_ShapePicker> picker_ {};
    vtkRenderer* renderer_ { nullptr };
    std::unique_ptr<GeometrySelectorHighlight> selector_ {};
};
#endif // GEOMETRY_SELECT_MANAGER_H
