/**
* @file：QRenderWindowStyle.h
* @brief：渲染窗口中的交互操作
* @author：付轩宇 982531420@qq.com

*/
#ifndef Q_RENDER_WINDOW_STYLE_H
#define Q_RENDER_WINDOW_STYLE_H
#include <vtkActor2D.h>
#include <vtkCellArray.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkProperty2D.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>

class SelectManager;
class InteractionService;

class QRenderWindowStyle : public vtkInteractorStyleTrackballCamera {
public:
	static QRenderWindowStyle* New();
    vtkTypeMacro(QRenderWindowStyle, vtkInteractorStyleTrackballCamera);
    void SetClick();
    void SetSelectManager(SelectManager* select_manager);
    //! @brief 交互服务激活时点击/悬停走交互拾取，否则走选择系统
    void SetInteractionService(InteractionService* service);
    void OnLeftButtonDown() override;
    void OnLeftButtonUp() override;
    void OnMouseMove() override;

private:
    //! @brief 取当前主渲染器（兜底用 DefaultRenderer / 第一个 renderer）
    vtkRenderer* currentRenderer();
    //! @brief 把橡皮筋 actor 添加到当前 renderer（box 开始时调用一次）
    void attachRubberBand();
    //! @brief 按当前 start/end 更新橡皮筋几何
    void updateRubberBand();
    //! @brief 移除橡皮筋 actor
    void detachRubberBand();

    bool click_ {};
    bool box_selecting_ {}; //> 框选拖拽中（Ctrl+左键）
    int box_start_[2] { 0, 0 };
    int box_end_[2] { 0, 0 };
    bool box_add_only_ {}; //> Shift 修饰：仅追加
    bool box_remove_only_ {}; //> Alt 修饰：仅移除

    // 橡皮筋：用 5 个顶点的折线（首末闭合）画矩形；actor2d 直接坐标用屏幕像素
    vtkNew<vtkPoints> rubber_band_points_;
    vtkNew<vtkCellArray> rubber_band_cells_;
    vtkNew<vtkPolyData> rubber_band_poly_;
    vtkNew<vtkPolyDataMapper2D> rubber_band_mapper_;
    vtkNew<vtkActor2D> rubber_band_actor_;
    bool rubber_band_attached_ {};

    SelectManager* select_manager_{};
    InteractionService* interaction_service_{};
};
#endif // Q_RENDER_WINDOW_STYLE_H
