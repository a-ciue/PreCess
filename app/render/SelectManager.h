#ifndef SELECT_MANAGER_H
#define SELECT_MANAGER_H
#include "Core.h"
#include "MeshSelectManager.h"
#include "Selection.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vtkNew.h>
#include <vtkActor.h>

class vtkRenderer;
class GeometrySelectManager;
class MeshActorManagerSelectOp;
class GeometryActorManagerSelectOp;
class ComponentSelectorHighlight;
class IMeshIdQuery;

class SelectManager {
public:
    SelectManager(vtkRenderer& renderer,
        MeshActorManagerSelectOp& mesh_op, GeometryActorManagerSelectOp& geom_op);
    ~SelectManager();
    void select(double posx, double posy);
    void setSelectMode(const std::string& select_mode);
    /**
     * @brief 设置网格面选择的角度扩散参数
     * @param enabled 是否启用按角度扩散
     * @param angle_deg 相邻面法向夹角阈值，单位为度
     */
    void setFaceSelectionByAngle(bool enabled, double angle_deg);
    //! @brief 吸附几何顶点：命中返回 {GeometryRegistry 顶点 id, 世界坐标}（转发给几何选择管理器）
    std::optional<std::pair<Index, std::array<double, 3>>> snapGeometryVertex(double posx, double posy);
    //! @brief 吸附网格顶点：命中返回组件 id、局部点 id 与世界坐标（转发给网格选择管理器）
    std::optional<MeshVertexSnap> snapMeshVertex(double posx, double posy);
    void clearSelection();
    //! @brief 注入模型层 id 查询接口，供选择器解析单元 id（如边端点对 -> 边 id）
    void setMeshIdQuery(const IMeshIdQuery* id_query);
    void refreshComponentHighlight();
    void setGeometryHighlightVisible(bool visible);
    void setGeometryHighlightVisible(Index component_id, bool visible);
    void setMeshHighlightVisible(bool visible);
    void setMeshHighlightVisible(Index component_id, bool visible);
    std::unique_ptr<Selection> getSelection();

private:
    std::unique_ptr<MeshSelectManager> mesh_;
    std::unique_ptr<GeometrySelectManager> geom_;
    std::unique_ptr<ComponentSelectorHighlight> component_selector_;
    SelectMode select_mode_ { SelectMode::None };
    vtkRenderer* renderer_ {};
    vtkNew<vtkActor> highlight_actor_;
};

#endif
