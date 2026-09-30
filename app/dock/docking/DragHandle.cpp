/**
 * @file DragHandle.cpp
 * @brief 拖拽源的实现
 */

#include "DragHandle.h"

#include "DockPanel.h"
#include "DockWindow.h"
#include "PanelGroup.h"

namespace dock {

DragHandle::DragHandle(DockView* view, PanelGroup* group, DockWindow* window,
    DockPanel* panel)
    : view_(view)
    , group_(group)
    , window_(window)
    , panel_(panel)
{
}

PanelGroup* DragHandle::group() const
{
    return group_;
}

DockWindow* DragHandle::window() const
{
    return window_;
}

DockPanel* DragHandle::panel() const
{
    return panel_;
}

bool DragHandle::isDetached() const
{
    return window_ != nullptr;
}

}
