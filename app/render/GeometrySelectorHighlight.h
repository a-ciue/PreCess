#ifndef GEOMETRY_SELECTOR_HIGHLIGHT_H
#define GEOMETRY_SELECTOR_HIGHLIGHT_H
#include "GeometryActorSelectOp.h"
#include "Selection.h"

#include <unordered_map>
#include <unordered_set>
#include <vtkSmartPointer.h>

class vtkActor;
class vtkPartitionedDataSet;
class vtkMapper;
class IVtkTools_SubPolyDataFilter;
class IVtkTools_ShapePicker;
struct GeometrySubshapeIndex;

using GeometrySelectionVtk = Selection;

class GeometrySelectorHighlight {
public:
    virtual ~GeometrySelectorHighlight() = default;
    virtual void toggle(IVtk_IdType subId, Index geomId) = 0;
    virtual void toggleSolid(GeomSolidId solidId, const std::vector<IVtk_IdType>& faceSubIds) = 0;
    virtual void clear() = 0;
    virtual GeometrySelectionVtk get() const = 0;
};

class GeometryFaceSelectorHighlight final : public GeometrySelectorHighlight {
public:
    static void setupHighlightStyle(vtkActor& actor, vtkMapper& mapper);

    GeometryFaceSelectorHighlight(vtkPartitionedDataSet& highlight_data,
        unsigned int partition_id, GeometryActorSelectOp select_op);
    ~GeometryFaceSelectorHighlight() override;
    void toggle(IVtk_IdType subId, Index geomId) override;
    void toggleSolid(GeomSolidId, const std::vector<IVtk_IdType>&) override { }
    void clear() override;
    GeometrySelectionVtk get() const override;

private:
    vtkPartitionedDataSet* highlight_data_;
    unsigned int partition_id_;
    GeometryActorSelectOp select_op_;
    vtkSmartPointer<IVtkTools_SubPolyDataFilter> hl_filter_;
    std::unordered_map<IVtk_IdType, Index> selections_;
};

class GeometryEdgeSelectorHighlight final : public GeometrySelectorHighlight {
public:
    static void setupHighlightStyle(vtkActor& actor, vtkMapper& mapper);

    GeometryEdgeSelectorHighlight(vtkPartitionedDataSet& highlight_data,
        unsigned int partition_id, GeometryActorSelectOp select_op);
    ~GeometryEdgeSelectorHighlight() override;
    void toggle(IVtk_IdType subId, Index geomId) override;
    void toggleSolid(GeomSolidId, const std::vector<IVtk_IdType>&) override { }
    void clear() override;
    GeometrySelectionVtk get() const override;

private:
    vtkPartitionedDataSet* highlight_data_;
    unsigned int partition_id_;
    GeometryActorSelectOp select_op_;
    vtkSmartPointer<IVtkTools_SubPolyDataFilter> hl_filter_;
    std::unordered_map<IVtk_IdType, Index> selections_;
};

class GeometryVertexSelectorHighlight final : public GeometrySelectorHighlight {
public:
    static void setupHighlightStyle(vtkActor& actor, vtkMapper& mapper);

    GeometryVertexSelectorHighlight(vtkPartitionedDataSet& highlight_data,
        unsigned int partition_id, GeometryActorSelectOp select_op);
    ~GeometryVertexSelectorHighlight() override;
    void toggle(IVtk_IdType subId, Index geomId) override;
    void toggleSolid(GeomSolidId, const std::vector<IVtk_IdType>&) override { }
    void clear() override;
    GeometrySelectionVtk get() const override;

private:
    vtkPartitionedDataSet* highlight_data_;
    unsigned int partition_id_;
    GeometryActorSelectOp select_op_;
    vtkSmartPointer<IVtkTools_SubPolyDataFilter> hl_filter_;
    std::unordered_map<IVtk_IdType, Index> selections_;
};

class GeometrySolidSelectorHighlight final : public GeometrySelectorHighlight {
public:
    static void setupHighlightStyle(vtkActor& actor, vtkMapper& mapper);

    GeometrySolidSelectorHighlight(vtkPartitionedDataSet& highlight_data,
        unsigned int partition_id, GeometryActorSelectOp select_op);
    ~GeometrySolidSelectorHighlight() override;
    void toggle(IVtk_IdType, Index) override { }
    void toggleSolid(GeomSolidId solidId, const std::vector<IVtk_IdType>& faceSubIds) override;
    void clear() override;
    GeometrySelectionVtk get() const override;

private:
    vtkPartitionedDataSet* highlight_data_;
    unsigned int partition_id_;
    GeometryActorSelectOp select_op_;
    vtkSmartPointer<IVtkTools_SubPolyDataFilter> hl_filter_;
    std::unordered_map<IVtk_IdType, Index> selections_;
    std::unordered_set<IVtk_IdType> highlighted_face_ids_;
};
#endif // GEOMETRY_SELECTOR_HIGHLIGHT_H
