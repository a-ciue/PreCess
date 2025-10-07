#ifndef MODEL_ACTOR_MANAGER_H
#define MODEL_ACTOR_MANAGER_H
#include "Core.h"
#include "MeshActor.h"
#include <unordered_map>

class vtkRenderer;

class MeshActorManager
{
public:
	void bindRender(vtkRenderer* renderer);
        std::shared_ptr<const MeshActor> getModelActor(Index model_id);
	void deleteModel(Index model_id);
	void loadModel(Index model_id, const MeshDataVtk& model_data, vtkRenderer* renderer, ModelRenderMode render_mode, bool edge_render);

	void setVisibility(Index model_id, bool visibility);
	void setRenderMode(Index model_id, ModelRenderMode render_mode);
	void setRenderEdge(Index model_id, bool is_render);
        /**
         * @brief 设置或取消裁剪平面
         * @param plane 裁剪平面，传入nullptr则取消裁剪
         */
        void setClipPlane(vtkPlane* plane);

	bool getCount(Index model_id);
	bool getIsEdgeRender(Index model_id);
	ModelRenderMode getMeshRenderMode(Index model_id);
private:
	std::unordered_map<Index, std::shared_ptr<MeshActor>> models_;
	vtkRenderer* renderer_{};
};
#endif