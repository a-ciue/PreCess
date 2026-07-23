#include "QRenderWindowStyle.h"
#include "InteractionService.h"
#include "SelectManager.h"
#include <vtkObjectFactory.h>
#include <vtkRenderWindowInteractor.h>

vtkStandardNewMacro(QRenderWindowStyle);

void QRenderWindowStyle::SetClick()
{
	click_ = true;
}

void QRenderWindowStyle::SetSelectManager(SelectManager* select_manager)
{
	this->select_manager_ = select_manager;
}

void QRenderWindowStyle::SetInteractionService(InteractionService* service)
{
	this->interaction_service_ = service;
}

void QRenderWindowStyle::OnLeftButtonUp()
{
	if (click_) {
		click_ = false;
		int pos[2];
		this->GetInteractor()->GetEventPosition(pos);
		if (interaction_service_ && interaction_service_->isActive())
			interaction_service_->pick(pos[0], pos[1]);
		else if (select_manager_)
			select_manager_->select(pos[0], pos[1]);
	}

	vtkInteractorStyleTrackballCamera::OnLeftButtonUp();
}

void QRenderWindowStyle::OnMouseMove()
{
	vtkInteractorStyleTrackballCamera::OnMouseMove();

	// 仅在无拖拽（非旋转/平移等相机操作）时做交互动态预览
	if (interaction_service_ && interaction_service_->isActive() && this->State == VTKIS_NONE) {
		int pos[2];
		this->GetInteractor()->GetEventPosition(pos);
		interaction_service_->hover(pos[0], pos[1]);
		this->GetInteractor()->Render();
	}
}
