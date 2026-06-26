#ifndef GEOMETRY_SELECTOR_HIGHLIGHT_H
#define GEOMETRY_SELECTOR_HIGHLIGHT_H
#include "GeometryActorSelectOp.h"
#include "Selection.h"

#include <unordered_map>
#include <unordered_set>
#include <vtkSmartPointer.h>
#include <IVtkTools_ShapePicker.hxx>

class vtkRenderer;
class vtkDataArray;
class IVtkTools_ShapePicker;
struct GeometrySubshapeIndex;

using GeometrySelectionVtk = Selection;

class GeometrySelectorHighlight {
public:
    virtual ~GeometrySelectorHighlight() = default;
    virtual void select(double posx, double posy) = 0;
    virtual void clear() = 0;
    virtual GeometrySelectionVtk get() const = 0;
    virtual void setCurGeomActor(GeometryActorSelectOpFactory geom_actor) = 0;
    virtual void setGeometryIndex(const GeometrySubshapeIndex* idx) = 0;
    void setPicker(vtkSmartPointer<IVtkTools_ShapePicker> picker) { picker_ = picker; }

protected:
    virtual void configurePicker() = 0;
    vtkSmartPointer<IVtkTools_ShapePicker> picker_;
};

class GeometryFaceSelectorHighlight final : public GeometrySelectorHighlight {
public:
    GeometryFaceSelectorHighlight(vtkRenderer* renderer);
    ~GeometryFaceSelectorHighlight() override;
    void clear() override;
    GeometrySelectionVtk get() const override;
    void select(double posx, double posy) override;
    void setCurGeomActor(GeometryActorSelectOpFactory geom_actor) override;
    void setGeometryIndex(const GeometrySubshapeIndex* idx) override;

private:
    void applyHighlight();
    void ensureTargetInit();
    void configurePicker() override;

    vtkRenderer* renderer_;
    GeometryActorSelectOpFactory geom_actor_;
    const GeometrySubshapeIndex* geom_index_ { nullptr };

    vtkSmartPointer<vtkDataArray> face_sub_id_arr_;
    unsigned char face_base_[3] { 200, 200, 200 };
    bool target_initialized_ { false };

    std::unordered_map<IVtk_IdType, Index> selections_;
};

class GeometryEdgeSelectorHighlight final : public GeometrySelectorHighlight {
public:
    GeometryEdgeSelectorHighlight(vtkRenderer* renderer);
    ~GeometryEdgeSelectorHighlight() override;
    void clear() override;
    GeometrySelectionVtk get() const override;
    void select(double posx, double posy) override;
    void setCurGeomActor(GeometryActorSelectOpFactory geom_actor) override;
    void setGeometryIndex(const GeometrySubshapeIndex* idx) override;

private:
    void applyHighlight();
    void ensureTargetInit();
    void configurePicker() override;

    vtkRenderer* renderer_;
    GeometryActorSelectOpFactory geom_actor_;
    const GeometrySubshapeIndex* geom_index_ { nullptr };

    vtkSmartPointer<vtkDataArray> line_sub_id_arr_;
    unsigned char line_base_[3] { 0, 0, 0 };
    bool target_initialized_ { false };

    std::unordered_map<IVtk_IdType, Index> selections_;
};

class GeometryVertexSelectorHighlight final : public GeometrySelectorHighlight {
public:
    GeometryVertexSelectorHighlight(vtkRenderer* renderer);
    ~GeometryVertexSelectorHighlight() override;
    void clear() override;
    GeometrySelectionVtk get() const override;
    void select(double posx, double posy) override;
    void setCurGeomActor(GeometryActorSelectOpFactory geom_actor) override;
    void setGeometryIndex(const GeometrySubshapeIndex* idx) override;

private:
    void applyHighlight();
    void ensureTargetInit();
    void configurePicker() override;

    vtkRenderer* renderer_;
    GeometryActorSelectOpFactory geom_actor_;
    const GeometrySubshapeIndex* geom_index_ { nullptr };

    vtkSmartPointer<vtkDataArray> line_sub_id_arr_;
    unsigned char line_base_[3] { 0, 0, 0 };
    bool target_initialized_ { false };

    std::unordered_map<IVtk_IdType, Index> selections_;
};

class GeometrySolidSelectorHighlight final : public GeometrySelectorHighlight {
public:
    GeometrySolidSelectorHighlight(vtkRenderer* renderer);
    ~GeometrySolidSelectorHighlight() override;
    void clear() override;
    GeometrySelectionVtk get() const override;
    void select(double posx, double posy) override;
    void setCurGeomActor(GeometryActorSelectOpFactory geom_actor) override;
    void setGeometryIndex(const GeometrySubshapeIndex* idx) override;

private:
    void applyHighlight();
    void ensureTargetInit();
    void configurePicker() override;

    vtkRenderer* renderer_;
    GeometryActorSelectOpFactory geom_actor_;
    const GeometrySubshapeIndex* geom_index_ { nullptr };

    vtkSmartPointer<vtkDataArray> face_sub_id_arr_;
    unsigned char face_base_[3] { 200, 200, 200 };
    bool target_initialized_ { false };

    std::unordered_map<IVtk_IdType, Index> selections_;
    std::unordered_set<IVtk_IdType> highlighted_face_ids_;
};
#endif // GEOMETRY_SELECTOR_HIGHLIGHT_H
