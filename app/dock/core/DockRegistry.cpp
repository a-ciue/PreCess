/**
 * @file DockRegistry.cpp
 * @brief 停靠对象注册表的实现
 */

#include "DockRegistry.h"

namespace dock {

DockRegistry& DockRegistry::self()
{
    static DockRegistry registry;
    return registry;
}

void DockRegistry::registerDockWidget(DockWidget* dock_widget)
{
    if (dock_widget && !dock_widgets_.contains(dock_widget))
        dock_widgets_.append(dock_widget);
}

void DockRegistry::unregisterDockWidget(DockWidget* dock_widget)
{
    dock_widgets_.removeOne(dock_widget);
}

void DockRegistry::registerFloatingWindow(FloatingWindow* floating_window)
{
    if (floating_window && !floating_windows_.contains(floating_window))
        floating_windows_.append(floating_window);
}

void DockRegistry::unregisterFloatingWindow(FloatingWindow* floating_window)
{
    floating_windows_.removeOne(floating_window);
}

}
