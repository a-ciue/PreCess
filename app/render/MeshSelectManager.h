#ifndef MESH_SELECT_MANAGER_H
#define MESH_SELECT_MANAGER_H
#include "Selection.h"
#include "SelectorHighlight.h"
#include "MeshActorSelectOp.h"
#include "Core.h"

#include <unordered_set>
#include <vtkNew.h>
#include <vtkAssembly.h>
#include <vtkCompositePolyDataMapper.h>

class MeshSelectManager {

public:
	void bindRenderer(vtkRenderer* renderer, vtkActor* highlight_actor);
	void select(double posx, double posy);
	void setSelectActor(std::weak_ptr<MeshActor> model_actor_);
	void setSelectMode(SelectMode select_mode);
	void clearSelection();
	std::unique_ptr<Selection>	getSelection();

private:
    std::optional<MeshActorSelectOpFactory> cur_model_actor_ {};
	SelectMode	select_mode_;
	vtkNew<vtkActor> selection_actor_;
	vtkNew<vtkPolyDataMapper> selection_mapper_;
	vtkRenderer* renderer_;
	vtkActor* highlight_actor_ {};
	std::unique_ptr<SelectorHighlight> selector_;
};
#endif
