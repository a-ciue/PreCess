/**
* @file：QRenderWindowStyle.h
* @brief：渲染窗口中的交互操作
* @author：付轩宇 982531420@qq.com

*/
#ifndef Q_RENDER_WINDOW_STYLE_H
#define Q_RENDER_WINDOW_STYLE_H
#include <vtkInteractorStyleTrackballCamera.h>

class SelectManager;

class QRenderWindowStyle : public vtkInteractorStyleTrackballCamera {
public:
	static QRenderWindowStyle* New();
    vtkTypeMacro(QRenderWindowStyle, vtkInteractorStyleTrackballCamera);
    void SetClick();
    void SetSelectManager(SelectManager* select_manager);
    void OnLeftButtonUp() override;
     
private:    
    bool click_ {};
    SelectManager* select_manager_{};
};
#endif // Q_RENDER_WINDOW_STYLE_H
