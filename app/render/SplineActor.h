#ifndef SPLINE_ACTOR_H
#define SPLINE_ACTOR_H
#include "Core.h"
#include "SplineDataVtk.h"
#include <vtkActor.h>
#include <vtkNew.h>
#include <vtkPolyDataMapper.h>

class SplineActor {

public:
    SplineActor(vtkRenderer* renderer, SplineRenderMode render_mode);

    SplineRenderMode getSplineRenderMode();
    bool getIsEdgeRender();

    void loadShape(const SplineDataVtk& spline_data);
    void deleteSplineActor();

    void setVisibility(bool visibility);
    void setRenderMode(SplineRenderMode render_mode);

private:
    SplineRenderMode render_mode_;
    bool edge_render;
    bool visibility_;
    std::unique_ptr<SplineDataVtk> spline_data_;

    vtkNew<vtkActor> poly_actor_;
    vtkNew<vtkActor> line_actor_;
    vtkRenderer* renderer_;
};

#endif