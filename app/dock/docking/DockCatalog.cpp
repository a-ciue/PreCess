/**
 * @file DockCatalog.cpp
 * @brief 停靠对象注册表的实现
 */

#include "DockCatalog.h"

#include "DockPanel.h"
#include "DockView.h"
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

bool DockCatalog::cyclePanel(bool forward)
{
    // 当前面板所在分组有多个显示面板：先循环切标签
    PanelGroup* group = focused_panel_ ? focused_panel_->group() : nullptr;
    if (group) {
        const QList<DockPanel*> shown = group->shownPanels();
        if (shown.size() > 1) {
            int index = shown.indexOf(group->activePanel());
            if (index < 0)
                index = 0;
            index = forward ? (index + 1) % shown.size()
                            : (index - 1 + shown.size()) % shown.size();
            group->setActivePanel(shown.at(index));
            focused_panel_ = shown.at(index);
            return true;
        }
    }

    // 否则按登记顺序切换到下一个显示中的面板
    QList<DockPanel*> shown_all;
    for (DockPanel* panel : panels_) {
        if (panel->isShown())
            shown_all.append(panel);
    }
    if (shown_all.isEmpty())
        return false;

    int index = shown_all.indexOf(focused_panel_);
    if (index < 0)
        index = forward ? -1 : 0;
    index = forward ? (index + 1) % shown_all.size()
                    : (index - 1 + shown_all.size()) % shown_all.size();

    DockPanel* next = shown_all.at(index);
    focused_panel_ = next;
    if (next->group()) {
        next->group()->setActivePanel(next);
        if (next->group()->view())
            next->group()->view()->bringToFront();
    }
    return true;
}

}
