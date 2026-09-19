/**
 * @file DragHandle.cpp
 * @brief 拖拽源的实现
 */

#include "DragHandle.h"

namespace dock {

DragHandle::DragHandle(DockView* view, PanelGroup* group, DockWindow* window,
    DockPanel* panel)
    : view_(view)
    , group_(group)
    , window_(window)
    , panel_(panel)
{
}

}
