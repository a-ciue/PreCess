/**
 * @file Draggable.cpp
 * @brief 拖拽源的实现
 */

#include "Draggable.h"

namespace dock {

Draggable::Draggable(View* view, Group* group, FloatingWindow* floating_window,
    DockWidget* dock_widget)
    : view_(view)
    , group_(group)
    , floating_window_(floating_window)
    , dock_widget_(dock_widget)
{
}

}
