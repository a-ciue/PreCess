#ifndef SPLINE_ACTOR_MANAGER_H
#define SPLINE_ACTOR_MANAGER_H
#include "Core.h"
#include <memory>
#include <unordered_map>
#include <vtkRenderer.h>

struct SplineDataVtk;
class SplineActor;

class SplineActorManager {
public:
    SplineActorManager();
    ~SplineActorManager();
    void bindRender(vtkRenderer* renderer);

    const SplineActor* getSplineActor(Index model_id);
    SplineRenderMode getSplineRenderMode(Index model_id);
    bool getIsEdgeRender(Index model_id);
    bool getCount(Index model_id);

    void deleteModel(Index model_id);
    void loadSpline(Index model_id, const SplineDataVtk& spline_data);

    void setVisibility(Index model_id, bool visibility);
    void setRenderMode(Index model_id, SplineRenderMode render_mode);

private:
    std::unordered_map<Index, std::unique_ptr<SplineActor>> models_;
    vtkRenderer* renderer_;
};

#endif