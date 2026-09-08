#include "QRenderWindowStyle.h"
#include "InteractionService.h"
#include "SelectManager.h"
#include <vtkObjectFactory.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRendererCollection.h>

vtkStandardNewMacro(QRenderWindowStyle);

QRenderWindowStyle::QRenderWindowStyle()
{
    // 橡皮筋几何/mapper/属性只初始化一次：多次框选复用同一 actor，避免每次 attach 重建
    rubber_band_points_->SetNumberOfPoints(5);
    rubber_band_cells_->InsertNextCell(5);
    for (vtkIdType i = 0; i < 5; ++i)
        rubber_band_cells_->InsertCellPoint(i);
    rubber_band_poly_->SetPoints(rubber_band_points_);
    rubber_band_poly_->SetLines(rubber_band_cells_);
    rubber_band_mapper_->SetInputData(rubber_band_poly_);
    rubber_band_actor_->SetMapper(rubber_band_mapper_);
    vtkNew<vtkProperty2D> prop;
    prop->SetColor(0.0, 1.0, 0.0);
    prop->SetLineWidth(2.0);
    rubber_band_actor_->SetProperty(prop);
}

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

vtkRenderer* QRenderWindowStyle::currentRenderer()
{
    auto* iren = this->GetInteractor();
    if (!iren)
        return nullptr;
    auto* rw = iren->GetRenderWindow();
    if (!rw)
        return nullptr;
    auto* ren = this->GetCurrentRenderer();
    if (!ren) {
        auto* renderers = rw->GetRenderers();
        ren = renderers ? renderers->GetFirstRenderer() : nullptr;
    }
    return ren;
}

void QRenderWindowStyle::attachRubberBand()
{
    if (rubber_band_attached_)
        return;
    auto* ren = currentRenderer();
    if (!ren)
        return;

    ren->AddActor2D(rubber_band_actor_);
    rubber_band_attached_ = true;
}

void QRenderWindowStyle::updateRubberBand()
{
    int xmin = std::min(box_start_[0], box_end_[0]);
    int xmax = std::max(box_start_[0], box_end_[0]);
    int ymin = std::min(box_start_[1], box_end_[1]);
    int ymax = std::max(box_start_[1], box_end_[1]);
    rubber_band_points_->SetPoint(0, xmin, ymin, 0.0);
    rubber_band_points_->SetPoint(1, xmax, ymin, 0.0);
    rubber_band_points_->SetPoint(2, xmax, ymax, 0.0);
    rubber_band_points_->SetPoint(3, xmin, ymax, 0.0);
    rubber_band_points_->SetPoint(4, xmin, ymin, 0.0);
    rubber_band_points_->Modified();
    if (auto* iren = this->GetInteractor())
        iren->Render();
}

void QRenderWindowStyle::detachRubberBand()
{
    if (!rubber_band_attached_)
        return;
    auto* ren = currentRenderer();
    if (ren)
        ren->RemoveActor2D(rubber_band_actor_);
    rubber_band_attached_ = false;
}

void QRenderWindowStyle::OnLeftButtonDown()
{
    auto* iren = this->GetInteractor();
    if (iren && iren->GetControlKey()) {
        // Ctrl+左键 → 进入框选模式；不调父类，相机不被旋转/平移
        box_selecting_ = true;
        iren->GetEventPosition(box_start_);
        box_end_[0] = box_start_[0];
        box_end_[1] = box_start_[1];
        attachRubberBand();
        updateRubberBand();
        return;
    }
    vtkInteractorStyleTrackballCamera::OnLeftButtonDown();
}

void QRenderWindowStyle::OnLeftButtonUp()
{
    if (box_selecting_) {
        // 矩形太小不触发（与 demo 对齐：>=2 像素）
        const int dx = std::abs(box_end_[0] - box_start_[0]);
        const int dy = std::abs(box_end_[1] - box_start_[1]);
        if (dx >= 2 && dy >= 2 && select_manager_) {
            int xmin = std::min(box_start_[0], box_end_[0]);
            int xmax = std::max(box_start_[0], box_end_[0]);
            int ymin = std::min(box_start_[1], box_end_[1]);
            int ymax = std::max(box_start_[1], box_end_[1]);
            select_manager_->selectArea(xmin, ymin, xmax, ymax);
        }
        detachRubberBand();
        box_selecting_ = false;
        // 吞掉 click_ 状态：框选路径不应触发点选
        click_ = false;
        return;
    }

    if (click_) {
        click_ = false;
        int pos[2];
        this->GetInteractor()->GetEventPosition(pos);
        if (interaction_service_ && interaction_service_->hasActiveState())
            interaction_service_->pick(pos[0], pos[1]);
        else if (select_manager_)
            select_manager_->select(pos[0], pos[1]);
    }

    vtkInteractorStyleTrackballCamera::OnLeftButtonUp();
}

void QRenderWindowStyle::OnMouseMove()
{
    vtkInteractorStyleTrackballCamera::OnMouseMove();

    if (box_selecting_) {
        // 更新橡皮筋；不进入交互动态预览（避免与框选拖动视觉冲突）
        this->GetInteractor()->GetEventPosition(box_end_);
        updateRubberBand();
        return;
    }

    // 仅在无拖拽（非旋转/平移等相机操作）时做交互动态预览
    if (interaction_service_ && interaction_service_->hasActiveState() && this->State == VTKIS_NONE) {
        int pos[2];
        this->GetInteractor()->GetEventPosition(pos);
        interaction_service_->hover(pos[0], pos[1]);
        this->GetInteractor()->Render();
    }
}