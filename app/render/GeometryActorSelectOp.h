#ifndef GEOMETRY_ACTOR_SELECT_OP_H
#define GEOMETRY_ACTOR_SELECT_OP_H
#include <memory>
#include <optional>
#include <vector>

#include <vtkSmartPointer.h>
#include <vtkActor.h>
#include <vtkPolyDataMapper.h>
#include <IVtkTools_SubPolyDataFilter.hxx>
#include <IVtk_Types.hxx>

#include "Core.h"
#include "GeometryActor.h"

class GeometryActorSelectOp;
class IVtkTools_ShapePicker;
class vtkRenderer;

struct GeometryHighlightPipeline {
    vtkSmartPointer<IVtkTools_SubPolyDataFilter> filter;
    vtkSmartPointer<vtkPolyDataMapper> mapper;
    vtkSmartPointer<vtkActor> actor;
};

class GeometryActorSelectOpFactory {
    friend GeometryActorSelectOp;

public:
    GeometryActorSelectOpFactory();
    GeometryActorSelectOpFactory(std::weak_ptr<GeometryActor> geometry_actor);
    std::optional<GeometryActorSelectOp> lock();

private:
    std::weak_ptr<GeometryActor> geometry_actor_;
};

class GeometryActorSelectOp {
    friend GeometryActorSelectOpFactory;

public:
    GeometryActorSelectOp(std::shared_ptr<GeometryActor> geometry_actor);

    void disablePickerModes(IVtkTools_ShapePicker* picker);

    void configurePicker(IVtkTools_ShapePicker* picker, SelectMode mode);

    std::optional<Index> pickSubshape(IVtkTools_ShapePicker* picker, vtkRenderer* renderer,
        double posx, double posy, SelectMode mode, IVtk_IdType& out_sub_id);

    bool pickSolid(IVtkTools_ShapePicker* picker, vtkRenderer* renderer, double posx, double posy,
        GeomSolidId& out_solid_id, std::vector<IVtk_IdType>& out_face_sub_ids);

    GeometryHighlightPipeline buildHighlight(SelectMode mode);

private:
    std::shared_ptr<GeometryActor> geometry_actor_;
};
#endif // GEOMETRY_ACTOR_SELECT_OP_H
