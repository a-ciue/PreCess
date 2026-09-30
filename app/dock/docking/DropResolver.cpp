/**
 * @file DropResolver.cpp
 * @brief 拖放落点解析的实现
 */

#include "DropResolver.h"

#include "DockCatalog.h"
#include "DockHost.h"
#include "DockRegion.h"
#include "DockView.h"
#include "DockWindow.h"
#include "PanelGroup.h"
#include "ZoneResolver.h"
#include "tree/LayoutNode.h"

namespace dock {

DropTarget DropResolver::resolve(const QPoint& global_pos, const DockRegion* dragged_region)
{
    DropTarget target;

    const auto consider = [&](DockRegion* area) -> bool {
        if (!area || area == dragged_region)
            return false;

        const QRect area_rect(area->globalOrigin(), area->geometry().size());

        if (PanelGroup* group = area->groupAt(global_pos)) {
            const QRect item_geometry = group->node()
                ? group->node()->geometry()
                : QRect();
            const QRect group_rect(area->globalOrigin() + item_geometry.topLeft(),
                item_geometry.size());
            target.region = area;
            target.group = group;

            // 标题栏/标签栏条带优先：视作合并，并按插入位置落点
            if (!group->isCentral() && group->view()) {
                const int tab_index = group->view()->tabInsertIndexAt(global_pos);
                if (tab_index >= 0) {
                    target.zone = DropZone::Merge;
                    target.tab_index = tab_index;
                    return true;
                }
            }

            // 内方框优先；未命中则回退到常显的外方框
            target.zone = ZoneResolver::zoneInGroup(group_rect, global_pos);
            // 中央持久分组不提供中心合并落点（对齐 KDDW NonDockable 语义）
            if (target.zone == DropZone::Merge && group->isCentral())
                target.zone = DropZone::None;
            if (target.zone == DropZone::None)
                target.zone = ZoneResolver::zoneInRegion(area_rect, global_pos);
            return true;
        }

        if (!area_rect.contains(global_pos))
            return false;

        // 区域内的任意位置都视为命中该停靠区域：
        // 不在分组上时由外指示器方框决定落点（未对准方框则无落点）
        target.region = area;
        target.group = nullptr;
        target.zone = ZoneResolver::zoneInRegion(area_rect, global_pos);
        return true;
    };

    const QList<DockWindow*>& floating_windows = DockCatalog::self().windows();
    for (auto it = floating_windows.crbegin(); it != floating_windows.crend() && !target.region; ++it)
        consider((*it)->region());
    if (!target.region) {
        if (DockHost* host = DockCatalog::self().host())
            consider(host->region());
    }

    return target;
}

}
