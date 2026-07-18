#ifndef GEOMETRY_ACTOR_H
#define GEOMETRY_ACTOR_H
#include "Core.h"
#include "GeometryDataVtk.h"
#include "GeometrySubshapeIndex.h"
#include <vtkActor.h>
#include <vtkDataArray.h>
#include <vtkNew.h>
#include <vtkSmartPointer.h>

#include <Standard_Handle.hxx>
#include <IVtkOCC_Shape.hxx>
typedef Handle(IVtkOCC_Shape) OccShapeHandle;

class GeometryActorSelectOp;
class vtkPolyData;

class GeometryActor {
    friend GeometryActorSelectOp;

public:
    GeometryActor(vtkRenderer* renderer, GeometryRenderMode render_mode);
    ~GeometryActor();

    GeometryRenderMode getGeometryRenderMode();
    bool getIsEdgeRender();

    void loadShape(const GeometryDataVtk& geometry_data);

    void setVisibility(bool visibility);
    bool isVisible() const;
    void setRenderMode(GeometryRenderMode render_mode);
    void setRenderEdge(bool is_render);

private:
    void deleteGeometryActor();

    GeometryRenderMode render_mode_;
    bool edge_render;
    bool visibility_;
    OccShapeHandle occ_shape_;
    const GeometrySubshapeIndex* geometry_index_;

    vtkNew<vtkActor> poly_actor_;
    vtkNew<vtkActor> line_actor_;
    vtkRenderer* renderer_;
    vtkSmartPointer<vtkDataArray> line_sub_id_array_;
    vtkSmartPointer<vtkDataArray> poly_sub_id_array_;

    vtkSmartPointer<vtkPolyData> poly_only_;
    vtkSmartPointer<vtkPolyData> line_only_;
};

#endif