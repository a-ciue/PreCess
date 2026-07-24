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

class MeshSelectManager {
public:
    MeshSelectManager(vtkRenderer& renderer, MeshActorManagerSelectOp& op);

    void select(double posx, double posy);
    void setSelectMode(SelectMode select_mode);
    void setFaceSelectionByAngle(bool enabled, double angle_deg);
    void clearSelection();
    std::unique_ptr<Selection> getSelection();
    void setHighlightVisible(bool visible);
    void setHighlightVisible(Index component_id, bool visible);

private:
    SelectorHighlight* getOrCreateSelector(Index component_id);
    void applyHighlightStyle(SelectMode mode);

    MeshActorManagerSelectOp* op_;
    SelectMode select_mode_ { SelectMode::None };
    vtkRenderer* renderer_ { };
    vtkSmartPointer<vtkHardwarePicker> component_picker_;

    vtkNew<vtkActor> highlight_actor_;
    vtkSmartPointer<vtkCompositePolyDataMapper> highlight_mapper_;
    vtkSmartPointer<vtkPartitionedDataSet> highlight_data_; //> 高亮部分的 Data

    FaceSelectionSpreadOptions face_selection_spread_options_;
    std::unordered_map<Index, std::unique_ptr<SelectorHighlight>> component_selectors_; //> 每个 component 对应的选择器
};

#endif
