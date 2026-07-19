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
class IMeshIdQuery;

class SelectManager {
public:
    SelectManager(vtkRenderer& renderer,
        MeshActorManagerSelectOp& mesh_op, GeometryActorManagerSelectOp& geom_op);
    ~SelectManager();
    void select(double posx, double posy);
    void setSelectMode(const std::string& select_mode);
    void clearSelection();
    //! @brief 注入模型层 id 查询接口，供选择器解析单元 id（如边端点对 -> 边 id）
    void setMeshIdQuery(const IMeshIdQuery* id_query);
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
