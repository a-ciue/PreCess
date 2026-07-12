#ifndef MESH_SELECT_MANAGER_H
#define MESH_SELECT_MANAGER_H
#include "Core.h"
#include "Selection.h"
#include "SelectorHighlight.h"
#include "MeshActorSelectOp.h"

#include <memory>
#include <unordered_map>
#include <vtkNew.h>
#include <vtkActor.h>
#include <vtkPolyDataMapper.h>

class vtkRenderer;
class MeshActorManagerSelectOp;

class MeshSelectManager {
public:
    MeshSelectManager(MeshActorManagerSelectOp& op);

    void bindRenderer(vtkRenderer* renderer, vtkActor* highlight_actor);
    void select(double posx, double posy);
    void setSelectMode(SelectMode select_mode);
    void clearSelection();
    std::unique_ptr<Selection> getSelection();

private:
    SelectorHighlight* getOrCreateSelector(Index component_id);

    MeshActorManagerSelectOp* op_;
    SelectMode select_mode_ { SelectMode::None };
    vtkRenderer* renderer_ {};
    vtkActor* highlight_actor_ {};
    std::unordered_map<Index, std::unique_ptr<SelectorHighlight>> component_selectors_;
};

#endif
