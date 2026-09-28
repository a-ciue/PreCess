/**
 * @file DragProxy.cpp
 * @brief 被拖拽浮动窗口的实现
 */

#include "DragProxy.h"

#include "DockWindow.h"

namespace dock {

DragProxy::DragProxy(DockWindow* window, const QPoint& press_pos)
    : window_(window)
    , press_pos_(press_pos)
    , anchor_pos_(window ? window->geometry().topLeft() : QPoint(0, 0))
    , position_(anchor_pos_)
{
}

DragProxy::~DragProxy() = default;

void DragProxy::moveTo(const QPoint& global_pos)
{
    if (!window_)
        return;

    position_ = anchor_pos_ + (global_pos - press_pos_);
    const QRect geometry(position_, window_->geometry().size());
    window_->setGeometry(geometry);
}

}
