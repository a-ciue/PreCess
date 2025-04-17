#ifndef SELECTMANAGER_H
#define SELECTMANAGER_H
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <vtkNew.h>
#include <vtkAssembly.h>
#include <vtkPropAssembly.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkCompositePolyDataMapper.h>

#include "Selection.h"
#include "Selector.h"
#include "Model.h"
#include "Style.h"
#include "Selection.h"
#include "ModelActor.h"

enum SelectMode {
	None,
	Face,
	Edge,
	Block,
};
class SelectManager {

public:
	void bindRenderer(vtkRenderer* renderer);

	void select(double posx, double posy);
	void setSelectActor(ModelActor* model_actor_);
	void setSelectMode(SelectMode select_mode);
	void clearSelection();
	std::unique_ptr<Selection>	getSelection();

private:
	ModelActor*	cur_model_actor_;
	SelectMode	select_mode_;
	vtkNew<vtkActor> selection_actor_;
	vtkNew<vtkMapper> selection_mapper_;
	vtkRenderer* renderer_;
	std::unique_ptr<SelectorHighlight> selector_;
};
#endif