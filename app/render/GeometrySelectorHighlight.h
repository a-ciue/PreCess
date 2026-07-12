#ifndef GEOMETRY_SELECTOR_HIGHLIGHT_H
#define GEOMETRY_SELECTOR_HIGHLIGHT_H
#include "GeometryActorSelectOp.h"
#include "Selection.h"

#include <unordered_map>
#include <unordered_set>
#include <vtkSmartPointer.h>
#include <IVtkTools_ShapePicker.hxx>

class vtkRenderer;
class vtkActor;
class IVtkTools_SubPolyDataFilter;
class IVtkTools_ShapePicker;
struct GeometrySubshapeIndex;

using GeometrySelectionVtk = Selection;

class GeometrySelectorHighlight {
public:
    virtual ~GeometrySelectorHighlight() = default;
    virtual void select(double posx, double posy) = 0;
    virtual void clear() = 0;
    virtual GeometrySelectionVtk get() const = 0;
};

class GeometryFaceSelectorHighlight final : public GeometrySelectorHighlight {
public:
    GeometryFaceSelectorHighlight(vtkRenderer& renderer, vtkActor& highlight_actor,
        GeometryActorSelectOp select_op, vtkSmartPointer<IVtkTools_ShapePicker> picker);
    ~GeometryFaceSelectorHighlight() override;
    void clear() override;
    GeometrySelectionVtk get() const override;
    void select(double posx, double posy) override;

private:
    vtkRenderer* renderer_;
    vtkActor* highlight_actor_;
    GeometryActorSelectOp select_op_;
    vtkSmartPointer<IVtkTools_ShapePicker> picker_;
    GeometryHighlightPipeline hl_;
    std::unordered_map<IVtk_IdType, Index> selections_;
};

class GeometryEdgeSelectorHighlight final : public GeometrySelectorHighlight {
public:
    GeometryEdgeSelectorHighlight(vtkRenderer& renderer, vtkActor& highlight_actor,
        GeometryActorSelectOp select_op, vtkSmartPointer<IVtkTools_ShapePicker> picker);
    ~GeometryEdgeSelectorHighlight() override;
    void clear() override;
    GeometrySelectionVtk get() const override;
    void select(double posx, double posy) override;

private:
    vtkRenderer* renderer_;
    vtkActor* highlight_actor_;
    GeometryActorSelectOp select_op_;
    vtkSmartPointer<IVtkTools_ShapePicker> picker_;
    GeometryHighlightPipeline hl_;
    std::unordered_map<IVtk_IdType, Index> selections_;
};

class GeometryVertexSelectorHighlight final : public GeometrySelectorHighlight {
public:
    GeometryVertexSelectorHighlight(vtkRenderer& renderer, vtkActor& highlight_actor,
        GeometryActorSelectOp select_op, vtkSmartPointer<IVtkTools_ShapePicker> picker);
    ~GeometryVertexSelectorHighlight() override;
    void clear() override;
    GeometrySelectionVtk get() const override;
    void select(double posx, double posy) override;

private:
    vtkRenderer* renderer_;
    vtkActor* highlight_actor_;
    GeometryActorSelectOp select_op_;
    vtkSmartPointer<IVtkTools_ShapePicker> picker_;
    GeometryHighlightPipeline hl_;
    std::unordered_map<IVtk_IdType, Index> selections_;
};

class GeometrySolidSelectorHighlight final : public GeometrySelectorHighlight {
public:
    GeometrySolidSelectorHighlight(vtkRenderer& renderer, vtkActor& highlight_actor,
        GeometryActorSelectOp select_op, vtkSmartPointer<IVtkTools_ShapePicker> picker);
    ~GeometrySolidSelectorHighlight() override;
    void clear() override;
    GeometrySelectionVtk get() const override;
    void select(double posx, double posy) override;

private:
    vtkRenderer* renderer_;
    vtkActor* highlight_actor_;
    GeometryActorSelectOp select_op_;
    vtkSmartPointer<IVtkTools_ShapePicker> picker_;
    GeometryHighlightPipeline hl_;
    std::unordered_map<IVtk_IdType, Index> selections_;
    std::unordered_set<IVtk_IdType> highlighted_face_ids_;
};
#endif // GEOMETRY_SELECTOR_HIGHLIGHT_H
