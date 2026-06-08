#ifndef SPLINE_ACTOR_H
#define SPLINE_ACTOR_H
#include "Core.h"
#include "GeometryDataVtk.h"
#include <vtkActor.h>
#include <vtkNew.h>
#include <vtkPolyDataMapper.h>

class GeometryActor {

public:
    GeometryActor(vtkRenderer* renderer, SplineRenderMode render_mode);
    ~GeometryActor();

    SplineRenderMode getSplineRenderMode();
    bool getIsEdgeRender();

    void loadShape(const GeometryDataVtk& spline_data);

    void setVisibility(bool visibility);
    void setRenderMode(SplineRenderMode render_mode);
    void setRenderEdge(bool is_render);

private:
    void deleteSplineActor();

    SplineRenderMode render_mode_;
    bool edge_render;
    bool visibility_;
    std::unique_ptr<GeometryDataVtk> spline_data_;

    vtkNew<vtkActor> poly_actor_;
    vtkNew<vtkActor> line_actor_;
    vtkRenderer* renderer_;
};

#endif