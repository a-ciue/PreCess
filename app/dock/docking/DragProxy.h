/**
 * @file DragProxy.h
 * @brief 正在被拖拽的浮动窗口：记录鼠标与窗口的偏移并跟随光标
 */

#pragma once

#include <QPoint>
#include <QRect>

namespace dock {

class DockWindow;

/**
 * @brief 拖拽会话中的浮动窗口
 *
 * DragSession 在拖拽开始时创建，负责把顶层窗口的位置随光标更新；
 * 视图层监听 DockWindow::geometryChanged 同步实际窗口。
 */
class DragProxy
{
public:
    DragProxy(DockWindow* window, const QPoint& press_pos);
    ~DragProxy();

    //! @brief 被拖拽的浮动窗口
    DockWindow* window() const { return window_; }

    //! @brief 跟随光标移动：窗口位置 = 起点 + (光标 - 按下点)
    void moveTo(const QPoint& global_pos);

private:
    DockWindow* window_ = nullptr;
    QPoint press_pos_;
    QPoint anchor_pos_;
    QPoint position_;
};

}
