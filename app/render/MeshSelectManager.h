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
    MeshSelectManager(vtkRenderer& renderer, vtkActor& highlight_actor, MeshActorManagerSelectOp& op);

    void select(double posx, double posy);
    void setSelectMode(SelectMode select_mode);
    void clearSelection();
    std::unique_ptr<Selection> getSelection();

private:
    SelectorHighlight* getOrCreateSelector(Index component_id);
    void applyHighlightStyle(SelectMode mode);

    MeshActorManagerSelectOp* op_;
    SelectMode select_mode_ { SelectMode::None };
    vtkRenderer* renderer_ { };
    vtkSmartPointer<vtkHardwarePicker> component_picker_;

    vtkActor* highlight_actor_ { }; //> 高亮部分的 Actor
    vtkSmartPointer<vtkCompositePolyDataMapper> highlight_mapper_; //> 高亮部分的 Mapper
    vtkSmartPointer<vtkPartitionedDataSet> highlight_data_; //> 高亮部分的 Data

    std::unordered_map<Index, std::unique_ptr<SelectorHighlight>> component_selectors_; //> 每个 component 对应的选择器
};

#endif
