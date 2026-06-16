#ifndef MODEL_ACTOR_MANAGER_H
#define MODEL_ACTOR_MANAGER_H
#include "Core.h"
#include "MeshActor.h"
#include <unordered_map>
#include <vtkPoints.h>

class vtkRenderer;

class MeshActorManager {
public:
    explicit MeshActorManager(vtkPoints* global_points);
    void bindRender(vtkRenderer* renderer);
    bool hasComponent(Index component_id) const;
    std::shared_ptr<const MeshActor> getComponentActor(Index component_id) const;
    void deleteComponent(Index component_id);
    void loadMesh(Index component_id, const MeshDataVtk& model_data,
        vtkRenderer* renderer, ModelRenderMode render_mode = ModelRenderMode::Face);

    void setVisibility(Index component_id, bool visibility);
    void setRenderMode(Index component_id, ModelRenderMode render_mode);
    void setRenderEdge(Index component_id, bool is_render);
    /**
     * @brief 设置或取消裁剪平面
     * @param plane 裁剪平面，传入nullptr则取消裁剪
     */
    void setClipPlane(vtkPlane* plane);

    bool getCount(Index component_id);
    bool getIsEdgeRender(Index component_id);
    ModelRenderMode getMeshRenderMode(Index component_id);

    void setAttriMode(
        Index component_id,
        const std::string& attr_name,
        Mode mode,
        std::map<std::string, std::any> args);
    void cancelAttri(Index component_id);

    void syncOriginalPointIds();

private:
    std::unordered_map<Index, std::shared_ptr<MeshActor>> component_actors_;
    vtkRenderer* renderer_ {};
    vtkPoints* global_points_ {};
};
#endif