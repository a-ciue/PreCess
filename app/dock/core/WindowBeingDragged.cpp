/**
 * @file WindowBeingDragged.cpp
 * @brief 被拖拽浮动窗口的实现
 */

#include "WindowBeingDragged.h"

#include "Draggable.h"
#include "FloatingWindow.h"

namespace dock {

WindowBeingDragged::WindowBeingDragged(Draggable* draggable, FloatingWindow* floating_window,
    const QPoint& press_pos)
    : draggable_(draggable)
    , floating_window_(floating_window)
    , press_pos_(press_pos)
    , anchor_pos_(floating_window ? floating_window->geometry().topLeft() : QPoint(0, 0))
    , position_(anchor_pos_)
{
}

WindowBeingDragged::~WindowBeingDragged() = default;

void WindowBeingDragged::moveTo(const QPoint& global_pos)
{
    if (!floating_window_)
        return;

    position_ = anchor_pos_ + (global_pos - press_pos_);
    const QRect geometry(position_, floating_window_->geometry().size());
    floating_window_->setGeometry(geometry);
}

}
