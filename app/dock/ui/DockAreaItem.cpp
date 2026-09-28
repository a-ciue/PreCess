/**
 * @file DockAreaItem.cpp
 * @brief 布局区域视图的实现
 */

#include "DockAreaItem.h"

#include "PanelGroupItem.h"
#include "DropZoneOverlay.h"
#include "DockRuntime.h"
#include "DividerItem.h"
#include "docking/ZoneGeometry.h"
#include "docking/DockMetrics.h"
#include "docking/DragSession.h"
#include "docking/DockRegion.h"
#include "docking/DockWindow.h"
#include "docking/PanelGroup.h"
#include "tree/LayoutNode.h"
#include "tree/BoxNode.h"
#include "tree/Divider.h"

#include <utility>

namespace dock::ui {

DockAreaItem::DockAreaItem(QQuickItem* parent)
    : QQuickItem(parent)
{
    // 拖拽结束（落点生效或取消）后布局可能已变化，统一重同步
    connect(&DragSession::self(), &DragSession::phaseChanged, this,
        [this](DragSession::Phase state) {
            if (state == DragSession::Phase::Idle)
                sync();
            updateZoneRects();
        });
    // 浮动/回停等命令式布局变化
    connect(&DragSession::self(), &DragSession::layoutChanged, this, &DockAreaItem::sync);
    connect(&DragSession::self(), &DragSession::zoneChanged, this, &DockAreaItem::updateZoneRects);
}

DockAreaItem::~DockAreaItem()
{
    if (auto* overlay = DockRuntime::instance().zonesOverlay())
        overlay->clear(this);

    // 分组视图所有权归 DockRuntime：析构前摘出，避免随本项被级联删除
    for (QQuickItem* child : childItems()) {
        if (auto* view = qobject_cast<PanelGroupItem*>(child))
            view->setParentItem(nullptr);
    }
}

void DockAreaItem::setRegion(DockRegion* region)
{
    if (region_ == region)
        return;

    if (region_)
        disconnect(region_, nullptr, this, nullptr);

    clearSeparatorViews();

    region_ = region;
    if (!region_) {
        // 区域解绑：清除可能残留的落点浮层（避免蓝框/标记留在屏幕上）
        if (auto* overlay = DockRuntime::instance().zonesOverlay())
            overlay->clear(this);
        return;
    }

    // 核心层先于视图销毁时（浮动窗口关闭），立即解除引用，避免悬空解引用
    connect(region_, &QObject::destroyed, this, [this] {
        region_ = nullptr;
        clearSeparatorViews();
        if (auto* overlay = DockRuntime::instance().zonesOverlay())
            overlay->clear(this);
    });
    sync();
}

void DockAreaItem::clearSeparatorViews()
{
    qDeleteAll(divider_views_);
    divider_views_.clear();
}

void DockAreaItem::sync()
{
    if (!region_ || syncing_)
        return;

    syncing_ = true;

#ifdef QT_DEBUG
    region_->validateTree();
#endif

    // 分组视图：按布局树叶子节点同步几何与可见性
    QSet<PanelGroup*> groups;
    const QList<PanelGroup*> region_groups = region_->groups();
    for (PanelGroup* group : region_groups)
        groups.insert(group);

    // 收口陈旧视图：已离开本区域的分组视图立即隐藏并摘除，交由目标区域接管；
    // 仅查询缓存，避免为已清空的分组误建视图
    const QSet<PanelGroup*> previous = synced_groups_;
    synced_groups_ = groups;
    for (PanelGroup* stale : previous) {
        if (groups.contains(stale))
            continue;
        if (PanelGroupItem* view = DockRuntime::instance().existingPanelGroupItem(stale)) {
            if (view->parentItem() == this) {
                view->setVisible(false);
                view->setParentItem(nullptr);
            }
        }
        if (const auto watch = group_watches_.take(stale); watch)
            disconnect(watch);
    }

    for (PanelGroup* group : std::as_const(groups)) {
        if (group_watches_.contains(group))
            continue;
        group_watches_.insert(group,
            connect(group, &QObject::destroyed, this, [this, group] {
                synced_groups_.remove(group);
                group_watches_.remove(group);
            }));
    }

    for (PanelGroup* group : region_groups) {
        PanelGroupItem* view = DockRuntime::instance().panelGroupItem(group);
        if (!view)
            continue;

        if (view->parentItem() != this)
            view->setParentItem(this);

        LayoutNode* item = group->node();
        if (item) {
            view->setPosition(item->geometry().topLeft());
            view->setSize(item->geometry().size());
            view->setVisible(item->isVisible());
        }
        // 显式 z 序：分组在下、分隔条在上，避免残留视图互相遮挡
        view->setZ(DockMetrics::kGroupLayerZ);
        // client 可能延迟注册（如中央持久部件）：每次同步刷新挂载与可见性
        view->syncFromGroup();
    }

    // 分隔条视图：创建/更新/回收
    QSet<Divider*> separators;
    collectSeparators(region_->rootNode(), separators);

    for (auto it = divider_views_.begin(); it != divider_views_.end();) {
        if (!separators.contains(it.key())) {
            delete it.value();
            it = divider_views_.erase(it);
        } else {
            ++it;
        }
    }

    for (Divider* separator : separators) {
        DividerItem* view = divider_views_.value(separator, nullptr);
        if (!view) {
            view = new DividerItem(this);
            view->setDivider(separator, this);
            divider_views_.insert(separator, view);
        }
        view->setPosition(separator->geometry().topLeft());
        view->setSize(separator->geometry().size());
        view->setVisible(true);
        view->setZ(DockMetrics::kSeparatorLayerZ);
    }

    updateZoneRects();
    syncing_ = false;
}

void DockAreaItem::collectSeparators(LayoutNode* item, QSet<Divider*>& separators) const
{
    if (!item || !item->isContainer())
        return;

    const auto* container = static_cast<BoxNode*>(item);
    for (Divider* separator : container->dividers())
        separators.insert(separator);
    for (LayoutNode* child : container->children())
        collectSeparators(child, separators);
}

void DockAreaItem::updateZoneRects()
{
    DropZoneOverlay* overlay = DockRuntime::instance().zonesOverlay();
    if (!overlay)
        return;

    DragSession& drag = DragSession::self();
    // region_ 为空说明核心对象已销毁（浮动窗口关闭），不得继续解引用
    if (!region_ || drag.phase() != DragSession::Phase::Dragging
        || drag.hoveredRegion() != region_) {
        overlay->clear(this);
        return;
    }

    const QRect area_rect(QPoint(0, 0), size().toSize());
    QRect group_rect = area_rect;
    if (drag.hoveredGroup() && drag.hoveredGroup()->node())
        group_rect = drag.hoveredGroup()->node()->geometry();

    const DropZone current = drag.hoveredZone();
    const bool has_group = drag.hoveredGroup() != nullptr;

    const QList<ZoneGeometry::ZoneRect> candidates
        = ZoneGeometry::allZones(area_rect, group_rect);
    const QRect area_global(region_->globalOrigin(), area_rect.size());

    // 外 4 方框常显（贴边可选，不依赖难以对准的空隙）；
    // 悬停面板时再叠加内 5 方框（上/下/左/右/合并）
    QList<DropZoneOverlay::ZoneRectHit> hits;
    for (const ZoneGeometry::ZoneRect& indicator : candidates) {
        const bool is_inner = isInnerZone(indicator.location);
        if (is_inner && !has_group)
            continue;
        // 中央持久分组不提供中心合并落点（对齐 KDDW NonDockable 语义）
        if (indicator.location == DropZone::Merge && has_group
            && drag.hoveredGroup()->isCentral())
            continue;

        const QRect global_rect(area_global.topLeft() + indicator.rect.topLeft(),
            indicator.rect.size());
        hits.append({ global_rect, indicator.location == current });
    }

    if (hits.isEmpty()) {
        overlay->clear(this);
        return;
    }

    // 目标分组描边：拖动悬停在面板上时高亮其整体范围
    QRect target_frame;
    if (has_group) {
        target_frame = QRect(area_global.topLeft() + group_rect.topLeft(), group_rect.size());
    }

    // 标签插入标记：悬停目标分组的标题栏/标签栏条带时绘制插入竖线
    QRect tab_insert_global;
    if (has_group && current == DropZone::Merge && drag.hoveredTabIndex() >= 0) {
        if (PanelGroupItem* group_view
            = DockRuntime::instance().panelGroupItem(drag.hoveredGroup())) {
            const QRect marker = group_view->tabInsertMarkerRect(drag.hoveredTabIndex());
            if (!marker.isNull()) {
                const QPointF top_left = group_view->mapToGlobal(marker.topLeft());
                tab_insert_global = QRect(top_left.toPoint(), marker.size());
            }
        }
    }

    overlay->showZoneRects(hits, area_global, this, target_frame, tab_insert_global);
}

}
