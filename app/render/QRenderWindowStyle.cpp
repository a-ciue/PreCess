#include "QRenderWindowStyle.h"
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

void QRenderWindowStyle::OnLeftButtonUp()
{
	if (click_) {
		click_ = false;
		int pos[2];
		this->GetInteractor()->GetEventPosition(pos);
		if (select_manager_)
			select_manager_->select(pos[0], pos[1]);
	}
		
	vtkInteractorStyleTrackballCamera::OnLeftButtonUp();
}
