#ifndef GEOMETRY_ACTOR_H
#define GEOMETRY_ACTOR_H
#include "Core.h"
#include "GeometryDataVtk.h"
#include "GeometrySubshapeIndex.h"
#include <vtkActor.h>
#include <vtkDataArray.h>
#include <vtkNew.h>
#include <vtkPolyDataMapper.h>
#include <vtkSmartPointer.h>

#include <Standard_Handle.hxx>
#include <IVtkOCC_Shape.hxx>
typedef Handle(IVtkOCC_Shape) OccShapeHandle;

class GeometryActor {

public:
    GeometryActor(vtkRenderer* renderer, GeometryRenderMode render_mode);
    ~GeometryActor();

    GeometryRenderMode getGeometryRenderMode();
    bool getIsEdgeRender();

    void loadShape(const GeometryDataVtk& geometry_data);

    void setVisibility(bool visibility);
    void setRenderMode(GeometryRenderMode render_mode);
    void setRenderEdge(bool is_render);

    vtkActor* polyActor() noexcept;
    const vtkActor* polyActor() const noexcept;
    vtkActor* lineActor() noexcept;
    const vtkActor* lineActor() const noexcept;
    const OccShapeHandle& getOccShape() const;
    const GeometrySubshapeIndex* geometryIndex() const noexcept;
    const vtkDataArray* lineSubIdArray() const noexcept;
    const vtkDataArray* polySubIdArray() const noexcept;

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
};

#endif