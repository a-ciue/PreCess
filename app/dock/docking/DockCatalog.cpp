/**
 * @file DockCatalog.cpp
 * @brief 停靠对象注册表的实现
 */

#include "DockCatalog.h"

#include "DockHost.h"
#include "DockPanel.h"
#include "DockRegion.h"
#include "DockWindow.h"
#include "PanelGroup.h"

namespace dock {

DockCatalog& DockCatalog::self()
{
    static DockCatalog registry;
    return registry;
}

void DockCatalog::registerPanel(DockPanel* panel)
{
    if (panel && !panels_.contains(panel))
        panels_.append(panel);
}

void DockCatalog::unregisterPanel(DockPanel* panel)
{
    panels_.removeOne(panel);
}

void DockCatalog::registerWindow(DockWindow* window)
{
    if (window && !windows_.contains(window))
        windows_.append(window);
}

void DockCatalog::unregisterWindow(DockWindow* window)
{
    windows_.removeOne(window);
}

PanelGroup* DockCatalog::groupAtGlobal(const QPoint& global_pos) const
{
    // 浮动窗口后进先出：最近生成的窗口在上层
    for (auto it = windows_.crbegin(); it != windows_.crend(); ++it) {
        if (DockWindow* window = *it) {
            if (DockRegion* region = window->region()) {
                if (PanelGroup* group = region->groupAt(global_pos))
                    return group;
            }
        }
    }

    if (host_ && host_->region())
        return host_->region()->groupAt(global_pos);
    return nullptr;
}

bool DockCatalog::cyclePanelAt(const QPoint& global_pos, bool forward)
{
    // 严格悬停驱动：仅当悬停分组有多标签时循环切换其标签
    PanelGroup* group = groupAtGlobal(global_pos);
    if (!group)
        return false;

    const QList<DockPanel*> shown = group->shownPanels();
    if (shown.size() <= 1)
        return false;

    int index = shown.indexOf(group->activePanel());
    if (index < 0)
        index = 0;
    index = forward ? (index + 1) % shown.size()
                    : (index - 1 + shown.size()) % shown.size();
    group->setActivePanel(shown.at(index));
    return true;
}

}
