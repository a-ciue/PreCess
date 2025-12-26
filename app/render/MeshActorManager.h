#ifndef MODEL_ACTOR_MANAGER_H
#define MODEL_ACTOR_MANAGER_H
#include "Core.h"
#include "MeshActor.h"
#include <unordered_map>

class vtkRenderer;

class MeshActorManager {
public:
    void bindRender(vtkRenderer* renderer);
    std::shared_ptr<const MeshActor> getModelActor(Index model_id);
    void deleteModel(Index model_id);
    void loadModel(Index model_id, const MeshDataVtk& model_data, vtkRenderer* renderer, ModelRenderMode render_mode);

    void setVisibility(Index model_id, bool visibility);
    void setRenderMode(Index model_id, ModelRenderMode render_mode);
    void setRenderEdge(Index model_id, bool is_render);
    void setRenderVertex(Index model_id, bool is_render);
    /**
     * @brief 设置或取消裁剪平面
     * @param plane 裁剪平面，传入nullptr则取消裁剪
     */
    void setClipPlane(vtkPlane* plane);

    bool getCount(Index model_id);
    bool getIsEdgeRender(Index model_id);
    bool getIsVertexRender(Index model_id);
    ModelRenderMode getMeshRenderMode(Index model_id);

    void setAttriMode(Index model_id, std::string attr_name, MeshActor::Mode mode, MeshActor::ElementType type, std::string texturePath);
    void cancelAttri(Index model_id);
    void setGlyph3DScaleFactor(Index model_id, double scale);
    void setScalarRange(Index model_id, double min, double max);
    void resetScalarRange(Index model_id);

private:
    std::unordered_map<Index, std::shared_ptr<MeshActor>> models_;
    vtkRenderer* renderer_ {};
};
#endif