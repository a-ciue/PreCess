#ifndef SPLINE_ACTOR_MANAGER_H
#define SPLINE_ACTOR_MANAGER_H
#include "Core.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <vtkRenderer.h>

struct GeometryDataVtk;
class GeometryActor;

class GeometryActorManager {
public:
    GeometryActorManager();
    ~GeometryActorManager();
    void bindRender(vtkRenderer* renderer);

    const GeometryActor* getSplineActor(Index component_id);
    SplineRenderMode getSplineRenderMode(Index component_id);
    bool getIsEdgeRender(Index component_id);
    bool hasComponent(Index component_id) const;

    void deleteComponent(Index component_id);
    void loadSpline(const GeometryDataVtk& spline_data);

    void setVisibility(Index component_id, bool visibility);
    void setRenderMode(Index component_id, SplineRenderMode render_mode);
    void setRenderEdge(Index component_id, bool is_render);

private:
    std::unordered_map<Index, std::unique_ptr<GeometryActor>> component_actors_;
    vtkRenderer* renderer_;
};

#endif