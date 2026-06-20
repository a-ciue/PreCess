#ifndef GEOMETRY_ACTOR_H
#define GEOMETRY_ACTOR_H
#include "Core.h"
#include "GeometryDataVtk.h"
#include <vtkActor.h>
#include <vtkNew.h>
#include <vtkPolyDataMapper.h>

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

private:
    void deleteGeometryActor();

    GeometryRenderMode render_mode_;
    bool edge_render;
    bool visibility_;
    std::unique_ptr<GeometryDataVtk> geometry_data_;

    vtkNew<vtkActor> poly_actor_;
    vtkNew<vtkActor> line_actor_;
    vtkRenderer* renderer_;
};

#endif