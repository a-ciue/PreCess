#ifndef MESH_SELECT_MANAGER_H
#define MESH_SELECT_MANAGER_H
#include "Core.h"
#include "Selection.h"
#include "SelectorHighlight.h"

#include <memory>
#include <unordered_map>
#include <vtkSmartPointer.h>

class vtkRenderer;
class vtkActor;
class vtkHardwarePicker;
class vtkCompositePolyDataMapper;
class vtkPartitionedDataSet;
class MeshActorManagerSelectOp;
class IMeshIdQuery;

//! @brief 网格顶点吸附结果（snapMeshVertex 返回值）
struct MeshVertexSnap {
    Index component_id { -1 };
    Index point_id { -1 }; //> 组件内局部点 id
    std::array<double, 3> world_pos {}; //> 数据集存储的精确坐标（同一顶点跨次拾取位级一致）
};

class MeshSelectManager {
public:
    MeshSelectManager(vtkRenderer& renderer, vtkActor& highlight_actor, MeshActorManagerSelectOp& op);

    void select(double posx, double posy);
    void setSelectMode(SelectMode select_mode);
    /**
     * @brief 设置面选择的角度扩散参数，并同步到已创建的面选择器
     * @param enabled 是否启用按角度扩散
     * @param angle_deg 相邻面法向夹角阈值，单位为度
     */
    void setFaceSelectionByAngle(bool enabled, double angle_deg);
    //! @brief 吸附网格顶点：命中返回组件 id、局部点 id 与世界坐标；未命中返回 std::nullopt
    std::optional<MeshVertexSnap> snapMeshVertex(double posx, double posy);
    void clearSelection();
    void setMeshIdQuery(const IMeshIdQuery* id_query);
    std::unique_ptr<Selection> getSelection();
    void setHighlightVisible(bool visible);
    void setHighlightVisible(Index component_id, bool visible);

private:
    SelectorHighlight* getOrCreateSelector(Index component_id);
    void applyHighlightStyle(SelectMode mode);

    MeshActorManagerSelectOp* op_;
    const IMeshIdQuery* id_query_ {};
    SelectMode select_mode_ { SelectMode::None };
    vtkRenderer* renderer_ { };
    vtkSmartPointer<vtkHardwarePicker> component_picker_;
    vtkSmartPointer<vtkHardwarePicker> vertex_picker_; //> 顶点吸附专用（SnapToMeshPoint，独立于组件拾取模式）

    vtkActor* highlight_actor_ { };
    vtkSmartPointer<vtkCompositePolyDataMapper> highlight_mapper_;
    vtkSmartPointer<vtkPartitionedDataSet> highlight_data_; //> 高亮部分的 Data
    bool highlight_visible_ { true };

    FaceSelectionSpreadOptions face_selection_spread_options_;
    std::unordered_map<Index, std::unique_ptr<SelectorHighlight>> component_selectors_; //> 每个 component 对应的选择器
};

#endif
