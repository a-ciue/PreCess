#ifndef SELECT_MANAGER_H
#define SELECT_MANAGER_H
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
#include "ModelActor.h"
#include "Core.h"

class SelectManager {

public:
	void bindRenderer(vtkRenderer* renderer);
	/**
	 * @brief 传入renderer
	 * 
	 * @param posx 
	 * @param posy 
	 */
	void select(double posx, double posy);

	void setSelectActor(ModelActor* model_actor_);
	/**
	 * @brief 传入选择模型
	 * @param select_mode 
	 */
	void setSelectMode(SelectMode select_mode);
	void clearSelection();
	std::unique_ptr<Selection>	getSelection();

private:
	ModelActor*	cur_model_actor_;
	SelectMode	select_mode_;
	vtkNew<vtkActor> selection_actor_;
	vtkNew<vtkPolyDataMapper> selection_mapper_;
	vtkRenderer* renderer_;
	std::unique_ptr<SelectorHighlight> selector_;
};
#endif