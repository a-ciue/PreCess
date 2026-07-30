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

    vtkActor* highlight_actor_ { };
    vtkSmartPointer<vtkCompositePolyDataMapper> highlight_mapper_;
    vtkSmartPointer<vtkPartitionedDataSet> highlight_data_; //> 高亮部分的 Data
    bool highlight_visible_ { true };

    FaceSelectionSpreadOptions face_selection_spread_options_;
    std::unordered_map<Index, std::unique_ptr<SelectorHighlight>> component_selectors_; //> 每个 component 对应的选择器
};

#endif
