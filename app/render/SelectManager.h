#ifndef SELECT_MANAGER_H
#define SELECT_MANAGER_H
#include "Core.h"
#include "Selection.h"

#include <memory>
#include <string>
#include <vtkNew.h>
#include <vtkActor.h>

class vtkRenderer;
class MeshSelectManager;
class GeometrySelectManager;
class MeshActorManagerSelectOp;
class GeometryActorManagerSelectOp;
class ComponentSelectorHighlight;

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
    void clearSelection();
    void refreshComponentHighlight();
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
