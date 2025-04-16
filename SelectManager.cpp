#include "SelectManager.h"
#include "ModelActor.h"
#include "Model.h"
#include <vtkActor.h>
#include <vtkCellArray.h>
#include <vtkMinimalStandardRandomSequence.h>
#include <vtkNamedColors.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkAssembly.h>
#include <vtkPropAssembly.h>
#include "ModelUtil.h"
#include "Style.h"
#include "Selection.h"
#include "Selector.h"
#include <vtkAppendPolyData.h>
#include <vtkCompositePolyDataMapper.h>
#include <vtkUnstructuredGrid.h>
#include <vtkTriangle.h>
#include <vtkMultiBlockDataSet.h>
using Index = int;

enum SelectMode {
	None,
	Face,
	Edge,
	Block,
};

void SelectManager::bindRenderer(vtkRenderer* renderer)
{
	this->renderer_ = renderer;
}

void SelectManager::select(double posx, double posy)
{
	this->selector_->select(posx, posy);
}

void SelectManager::setSelectActor(ModelActor* model_actor_)
{
	this->cur_model_actor_ = model_actor_;
	this->cur_model_actor_->addPickList(this->selector_->getPickList());
}

void SelectManager::setSelectMode(SelectMode select_mode)
{
	this->select_mode_=select_mode;
	if (this->select_mode_ = SelectMode::Face)
	{
		this->selector_ = std::make_unique<SingleFaceSelectorHighlight>(this->renderer_);
	}
	else if (this->select_mode_ = SelectMode::Block)
	{
		this->selector_ = std::make_unique<BlockSelectorHighlight>(this->renderer_);
	}
	else if (this->select_mode_ = SelectMode::Edge)
	{
		this->selector_ = std::make_unique<SingleFaceSelectorHighlight>(this->renderer_);
	}
	else
	{
		this->selector_->clear();
	}
}

void SelectManager::clearSelection()
{
	this->selector_->clear();
}

std::unique_ptr<Selection> SelectManager::getSelection()
{
	this->selector_->get();
	return std::unique_ptr<Selection>();
}
