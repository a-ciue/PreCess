/**
 * @file Draggable.cpp
 * @brief 拖拽源的实现
 */

#include "Draggable.h"

namespace dock {

Draggable::Draggable(View* view, Group* group, FloatingWindow* floating_window)
    : view_(view)
    , group_(group)
    , floating_window_(floating_window)
{
}

}
