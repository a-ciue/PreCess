/**
* @file：QRenderWindowStyle.h
* @brief：渲染窗口中的交互操作
* @author：付轩宇 982531420@qq.com

*/
#ifndef Q_RENDER_WINDOW_STYLE_H
#define Q_RENDER_WINDOW_STYLE_H
#include <vtkInteractorStyleTrackballCamera.h>

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
    void OnLeftButtonUp() override;
    void OnMouseMove() override;

private:
    bool click_ {};
    SelectManager* select_manager_{};
    InteractionService* interaction_service_{};
};
#endif // Q_RENDER_WINDOW_STYLE_H
