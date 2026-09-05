#ifndef MODEL_ACTOR_MANAGER_H
#define MODEL_ACTOR_MANAGER_H
#include "Core.h"
#include "MeshActor.h"
#include "MeshActorManagerSelectOp.h"
#include "TopologyDiagnosticCategory.h"
#include <unordered_map>
#include <array>
#include <vtkScalarBarActor.h>

class vtkRenderer;

class MeshActorManager {
public:
    MeshActorManager();
    void bindRender(vtkRenderer* renderer);
    bool hasComponent(Index component_id) const;
    std::shared_ptr<MeshActor> getComponentActor(Index component_id) const;
    void deleteComponent(Index component_id);
    void loadMesh(Index component_id, const MeshDataVtk& model_data,
        vtkRenderer* renderer);

    void setVisibility(Index component_id, bool visibility);
    void setClipPlane(vtkPlane* plane);
    void setCurrentRenderStyle(MeshRenderStyle style);
    MeshRenderStyle getCurrentRenderStyle() const;

    bool getCount(Index component_id);

    void setAttriMode(
        Index component_id,
        const std::string& attr_name,
        Mode mode,
        std::map<std::string, std::any> args);
    void cancelAttri(Index component_id);

    /** @brief 设置窗口级拓扑诊断类别是否启用；Actor 最终显隐仍跟随所属 MeshActor */
    void setTopologyDiagnosticCategoryEnabled(int category, bool enabled);
    /** @brief 设置二面角诊断边的筛选范围，单位为度 */
    void setDihedralAngleRange(double minimum, double maximum);

    MeshActorManagerSelectOp& op() { return op_; }
    const MeshActorManagerSelectOp& op() const { return op_; }

private:
    MeshActorManagerSelectOp op_ { *this };
    std::unordered_map<Index, std::shared_ptr<MeshActor>> component_actors_;
    vtkRenderer* renderer_ {};
    MeshRenderStyle current_style_ { MeshRenderStyle::FaceWithEdges };
    std::array<bool, kTopologyDiagnosticCategoryCount> topology_diagnostic_category_enabled_ {};
    double dihedral_minimum_ { 0.0 };
    double dihedral_maximum_ { 150.0 };

    // 渲染窗口共享的标量颜色表，同一时刻只显示当前标量属性的颜色和值域。
    vtkNew<vtkScalarBarActor> scalar_bar_;

    // 当前颜色表对应的组件 ID，未显示颜色表时为 -1。
    Index scalar_bar_component_id_ { -1 };
};
#endif
