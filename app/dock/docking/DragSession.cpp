/**
 * @file DragSession.cpp
 * @brief 拖拽状态机的实现
 */

#include "DragSession.h"

#include "DockMetrics.h"
#include "DockCatalog.h"
#include "DockPanel.h"
#include "DragHandle.h"
#include "DockRegion.h"
#include "ZoneResolver.h"
#include "DockWindow.h"
#include "PanelGroup.h"
#include "DockHost.h"
#include "DockView.h"
#include "DragProxy.h"
#include "tree/LayoutNode.h"
#include "tree/BoxNode.h"

#include <utility>

namespace dock {

namespace {

//! @brief 布局树中是否包含指定节点
bool treeContains(const LayoutNode* root, const LayoutNode* item)
{
    if (!root)
        return false;
    if (root == item)
        return true;
    if (!root->isContainer())
        return false;

    const auto* container = static_cast<const BoxNode*>(root);
    for (LayoutNode* child : container->children()) {
        if (treeContains(child, item))
            return true;
    }
    return false;
}

}

DragSession& DragSession::self()
{
    static DragSession controller;
    return controller;
}

DragSession::DragSession(QObject* parent)
    : QObject(parent)
{
}

DragSession::~DragSession()
{
    cleanup();
}

DockWindow* DragSession::dragWindow() const
{
    return drag_proxy_ ? drag_proxy_->window() : nullptr;
}

void DragSession::beginAt(DragHandle* handle, const QPoint& global_pos)
{
    if (!handle || !handle->group())
        return;

    // 中央持久分组/面板不可拖拽（其节点受区域保护，拖出会破坏布局树）
    if (handle->group()->isCentral()
        || (handle->panel() && handle->panel()->isCentral()))
        return;

    if (phase_ != Phase::Idle)
        cancel();

    phase_ = Phase::Armed;
    handle_ = handle;
    press_pos_ = global_pos;
    Q_EMIT phaseChanged(phase_);
}

void DragSession::updateAt(const QPoint& global_pos)
{
    if (phase_ == Phase::Armed) {
        if ((global_pos - press_pos_).manhattanLength() < DockMetrics::kStartDragDistance)
            return;
        startDrag(global_pos);
        return;
    }

    if (phase_ == Phase::Dragging) {
        // 防御：Dragging 仅由 startDrag 创建拖拽代理后置位，代理应恒非空
        if (!drag_proxy_)
            return;
        drag_proxy_->moveTo(global_pos);
        updateHover(global_pos);
    }
}

void DragSession::endAt(const QPoint& global_pos)
{
    if (phase_ != Phase::Dragging) {
        cleanup();
        return;
    }

    updateHover(global_pos);
    if (hovered_region_ && hovered_zone_ != DropZone::None) {
        applyDrop();
    } else if (drag_group_ && handle_ && drag_group_ != handle_->group() && origin_group_) {
        // 未命中落点：单标签浮出保留为浮动窗口，登记回停来源
        parkFloatingGroup(dragWindow());
    }

    cleanup();
}

void DragSession::cancel()
{
    if (phase_ == Phase::Dragging && drag_group_ && handle_
        && drag_group_ != handle_->group()) {
        // 单标签拖拽取消：归还源分组
        DockWindow* window = dragWindow();
        PanelGroup* temp_group = drag_group_;
        DockPanel* dock = dragged_panel_;

        if (window && window->group() == temp_group)
            window->releaseGroup();

        if (dock) {
            temp_group->removePanel(dock);
            if (origin_group_) {
                origin_group_->addPanel(dock);
                origin_group_->setActivePanel(dock);
            }
        }

        LayoutNode* item = temp_group->node();
        delete item;
        temp_group->setNode(nullptr);
        if (window)
            destroyDragWindow(window);

        floating_origins_.remove(temp_group);
        temp_group->deleteLater();
        Q_EMIT layoutChanged();
    } else if (phase_ == Phase::Dragging && handle_ && handle_->group()
        && !handle_->isDetached()) {
        reattachGroup(handle_->group());
    }

    cleanup();
}

bool DragSession::detachGroup(PanelGroup* group)
{
    DockHost* host = DockCatalog::self().host();
    if (!host || !group || group->vacancy() || group->isCentral())
        return false;

    DockRegion* area = host->region();
    LayoutNode* item = area->extractGroupForWindow(group);
    if (!item)
        return false;

    DockWindow* window = createDragWindow();
    const QRect target(area->globalOrigin() + item->geometry().topLeft(), item->geometry().size());
    window->takeGroup(group, item);
    window->setGeometry(target);
    Q_EMIT layoutChanged();
    return true;
}

bool DragSession::reattachGroup(PanelGroup* group)
{
    if (!group)
        return false;

    // 单标签浮出：归还源分组（无占位可恢复）
    const auto origin_it = floating_origins_.constFind(group);
    if (origin_it != floating_origins_.constEnd()) {
        PanelGroup* origin = origin_it->origin_group;
        if (!origin)
            return false;

        const QList<DockPanel*> docks = group->panels();
        if (docks.isEmpty())
            return false;
        DockPanel* dock = docks.first();

        DockWindow* window = windowForGroup(group);
        if (window)
            window->releaseGroup();

        group->removePanel(dock);
        origin->addPanel(dock);
        origin->setActivePanel(dock);

        LayoutNode* item = group->node();
        delete item;
        group->setNode(nullptr);
        floating_origins_.remove(group);
        if (window)
            destroyDragWindow(window);

        Q_EMIT layoutChanged();
        return true;
    }

    DockHost* host = DockCatalog::self().host();
    if (!host)
        return false;

    DockWindow* window = windowForGroup(group);
    if (window)
        window->releaseGroup(); // 摘出布局节点，浮窗变空

    const bool restored = host->region()->restoreGroupFromWindow(group);
    if (window)
        destroyDragWindow(window);
    if (restored)
        Q_EMIT layoutChanged();
    return restored;
}

bool DragSession::toggleDetached(PanelGroup* group)
{
    if (!group)
        return false;
    if (group->vacancy() || floating_origins_.contains(group))
        return reattachGroup(group);
    return detachGroup(group);
}

void DragSession::startDrag(const QPoint& global_pos)
{
    if (!handle_ || !handle_->group()) {
        cleanup();
        return;
    }

    PanelGroup* source_group = handle_->group();
    DockPanel* dock = handle_->panel();
    // 防御：中央持久分组/面板不得进入拖拽流程
    if (source_group->isCentral() || (dock && dock->isCentral())) {
        cleanup();
        return;
    }

    const bool single_tab = dock && source_group->shownPanels().size() > 1;

    QSize size;
    if (handle_->window())
        size = handle_->window()->geometry().size();
    else if (source_group->node())
        size = source_group->node()->geometry().size();
    if (size.isEmpty())
        size = QSize(300, 200);

    DockRegion* source_area = regionForGroup(source_group);
    DockWindow* window = nullptr;

    if (single_tab) {
        // 从分组中摘出该标签，放入临时分组并浮出
        origin_group_ = source_group;
        origin_index_ = source_group->shownPanels().indexOf(dock);
        source_group->removePanel(dock);

        drag_group_ = new PanelGroup(this);
        drag_group_->addPanel(dock);
        auto* item = new LayoutNode(drag_group_);
        drag_group_->setNode(item);

        window = createDragWindow();
        const QPoint window_pos = global_pos - QPoint(size.width() / 2, DockMetrics::kTitleBarHeight / 2);
        window->takeGroup(drag_group_, item);
        window->setGeometry(QRect(window_pos, size));
        dragged_panel_ = dock;
    } else {
        drag_group_ = source_group;
        if (handle_->window()) {
            window = handle_->window();
            source_area = window->region();
        } else {
            DockHost* host = DockCatalog::self().host();
            if (!host) {
                cleanup();
                return;
            }

            source_area = host->region();
            LayoutNode* item = source_area->extractGroupForWindow(source_group);
            if (!item) {
                cleanup();
                return;
            }

            window = createDragWindow();
            const QPoint window_pos = global_pos
                - QPoint(size.width() / 2, DockMetrics::kTitleBarHeight / 2);
            window->takeGroup(source_group, item);
            window->setGeometry(QRect(window_pos, size));
        }
    }

    source_region_ = source_area;
    drag_proxy_ = new DragProxy(handle_, window, global_pos);

    phase_ = Phase::Dragging;
    Q_EMIT phaseChanged(phase_);
    updateHover(global_pos);
}

void DragSession::parkFloatingGroup(DockWindow* window)
{
    if (!window || !drag_group_)
        return;

    const FloatOrigin origin { origin_group_, origin_index_ };
    floating_origins_.insert(drag_group_, origin);
    drag_group_->setParent(window);

    PanelGroup* parked = drag_group_;
    QObject::connect(parked, &QObject::destroyed, this, [this, parked] {
        floating_origins_.remove(parked);
    });
}

void DragSession::updateHover(const QPoint& global_pos)
{
    DockRegion* found_area = nullptr;
    PanelGroup* found_group = nullptr;
    DropZone found_location = DropZone::None;

    const DockWindow* dragged_window = drag_proxy_
        ? drag_proxy_->window()
        : nullptr;
    const DockRegion* dragged_area = dragged_window ? dragged_window->region() : nullptr;

    const auto consider = [&](DockRegion* area) -> bool {
        if (!area || area == dragged_area)
            return false;

        const QRect area_rect(area->globalOrigin(), area->geometry().size());

        if (PanelGroup* group = area->groupAt(global_pos)) {
            const QRect item_geometry = group->node()
                ? group->node()->geometry()
                : QRect();
            const QRect group_rect(area->globalOrigin() + item_geometry.topLeft(),
                item_geometry.size());
            found_area = area;
            found_group = group;
            // 内方框优先；未命中则回退到常显的外方框
            found_location = ZoneResolver::zoneInGroup(group_rect, global_pos);
            // 中央持久分组不提供中心合并落点（对齐 KDDW NonDockable 语义）
            if (found_location == DropZone::Merge && group->isCentral())
                found_location = DropZone::None;
            if (found_location == DropZone::None)
                found_location = ZoneResolver::zoneInRegion(area_rect, global_pos);
            return true;
        }

        if (!area_rect.contains(global_pos))
            return false;

        // 区域内的任意位置都视为命中该停靠区域：
        // 不在分组上时由外指示器方框决定落点（未对准方框则无落点）
        found_area = area;
        found_group = nullptr;
        found_location = ZoneResolver::zoneInRegion(area_rect, global_pos);
        return true;
    };

    const QList<DockWindow*>& floating_windows = DockCatalog::self().windows();
    for (auto it = floating_windows.crbegin(); it != floating_windows.crend() && !found_area; ++it)
        consider((*it)->region());
    if (!found_area) {
        if (DockHost* host = DockCatalog::self().host())
            consider(host->region());
    }

    if (found_area == hovered_region_ && found_group == hovered_group_
        && found_location == hovered_zone_)
        return;

    hovered_region_ = found_area;
    hovered_group_ = found_group;
    hovered_zone_ = found_location;
    Q_EMIT zoneChanged();
}

void DragSession::applyDrop()
{
    PanelGroup* group = drag_group_ ? drag_group_ : (handle_ ? handle_->group() : nullptr);
    if (!group || !hovered_region_)
        return;

    DockWindow* window = dragWindow();
    const bool temp_group = drag_group_ && handle_ && drag_group_ != handle_->group();

    if (hovered_zone_ == DropZone::Merge) {
        if (!hovered_group_ || hovered_group_ == group || hovered_group_->isCentral())
            return;

        // 全部面板并入目标分组
        const QList<DockPanel*> docks = group->panels();
        for (DockPanel* panel : docks) {
            hovered_group_->addPanel(panel);
            panel->applyDetached(false);
        }
        for (DockPanel* panel : std::as_const(docks))
            group->removePanel(panel);

        if (source_region_)
            source_region_->discardGroupVacancy(group);

        LayoutNode* item = window && window->group() == group
            ? window->releaseGroup()
            : group->node();
        delete item;
        group->setNode(nullptr);

        if (temp_group) {
            floating_origins_.remove(group);
            group->deleteLater(); // 临时分组：面板已并入目标
        }
    } else {
        if (source_region_)
            source_region_->discardGroupVacancy(group);

        LayoutNode* item = group->node();
        if (window && window->group() == group)
            item = window->releaseGroup(); // 从浮窗摘出，所有权在本控制器
        if (item) {
            group->setNode(item);
            // 空尺寸 = 无期望尺寸：按公平份额分空间（两项时各占一半）
            hovered_region_->attachGroup(group, hovered_zone_, hovered_group_, QSize());
            if (temp_group) {
                // 成为分栏：不再需要回停来源
                floating_origins_.remove(group);
                group->setParent(hovered_region_);
            }
        }
    }

    destroyDragWindow(window);
}

void DragSession::cleanup()
{
    delete drag_proxy_;
    drag_proxy_ = nullptr;
    handle_ = nullptr;
    source_region_ = nullptr;
    dragged_panel_ = nullptr;
    drag_group_ = nullptr;
    origin_group_ = nullptr;
    origin_index_ = -1;

    if (phase_ != Phase::Idle) {
        phase_ = Phase::Idle;
        Q_EMIT phaseChanged(phase_);
    }

    if (hovered_region_ || hovered_zone_ != DropZone::None) {
        hovered_region_ = nullptr;
        hovered_group_ = nullptr;
        hovered_zone_ = DropZone::None;
        Q_EMIT zoneChanged();
    }
}

DockWindow* DragSession::createDragWindow()
{
    auto* window = new DockWindow();

    DockHost* host = DockCatalog::self().host();
    if (host && host->view()) {
        if (DockView* view = host->view()->createDockWindow(window))
            window->setView(view);
    }
    return window;
}

void DragSession::destroyDragWindow(DockWindow* window)
{
    if (!window)
        return;

    window->close();
    delete window;
}

DockWindow* DragSession::windowForGroup(PanelGroup* group)
{
    for (DockWindow* window : DockCatalog::self().windows()) {
        if (window->group() == group)
            return window;
    }
    return nullptr;
}

DockRegion* DragSession::regionForGroup(PanelGroup* group)
{
    if (!group || !group->node())
        return nullptr;

    const LayoutNode* target = group->node();
    if (DockHost* host = DockCatalog::self().host()) {
        if (treeContains(host->region()->rootNode(), target))
            return host->region();
    }
    for (DockWindow* window : DockCatalog::self().windows()) {
        if (treeContains(window->region()->rootNode(), target))
            return window->region();
    }
    return nullptr;
}

}
