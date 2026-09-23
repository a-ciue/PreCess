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
#include "DropResolver.h"
#include "DockWindow.h"
#include "PanelGroup.h"
#include "DockHost.h"
#include "DockView.h"
#include "DragProxy.h"
#include "tree/LayoutNode.h"
#include "tree/BoxNode.h"

#include <utility>

namespace dock {

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

    // 能力门控：单标签拖动按面板能力、整组拖动按组能力（与视图层入口一致）
    const bool movable = handle->panel()
        ? handle->panel()->hasFeature(DockPanel::Feature::Movable)
        : handle->group()->features().testFlag(DockPanel::Feature::Movable);
    if (!movable)
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
        if (!DockMetrics::exceedsDragThreshold(global_pos - press_pos_))
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
    const bool dropped
        = hover_.region && hover_.zone != DropZone::None && applyDrop();
    if (!dropped) {
        if (drag_group_ && !drag_group_->features().testFlag(DockPanel::Feature::Floatable)) {
            // 未生效落点且分组不可浮动：整组回弹/单标签归还源分组，不保留浮动
            cancel();
            return;
        }
        if (drag_group_ && handle_ && drag_group_ != handle_->group() && origin_group_) {
            // 未命中落点：可浮动则保留为浮动窗口
            parkFloatingGroup(dragWindow());
        }
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

        disposeGroupNode(temp_group, window);
        if (window)
            closeWindowIfEmpty(window);

        floating_origins_.remove(temp_group);
        temp_group->setParent(this); // 脱离浮窗父子关系后再延迟销毁
        temp_group->deleteLater();
        Q_EMIT layoutChanged();
    } else if (phase_ == Phase::Dragging && handle_ && handle_->group()
        && !handle_->isDetached()) {
        reattachGroup(handle_->group());
    }

    cleanup();
}

void DragSession::resetSessionMembers()
{
    delete drag_proxy_;
    drag_proxy_ = nullptr;
    handle_ = nullptr;
    source_region_ = nullptr;
    dragged_panel_ = nullptr;
    drag_group_ = nullptr;
    origin_group_ = nullptr;
    origin_index_ = -1;
}

void DragSession::abort()
{
    if (!handle_ && !drag_proxy_ && !drag_group_ && phase_ == Phase::Idle
        && !hover_.region && hover_.zone == DropZone::None) {
        return;
    }

    // 宿主拆解期：只重置会话状态，不触碰可能正在销毁的模型对象，也不发信号
    resetSessionMembers();
    phase_ = Phase::Idle;
    hover_ = DropTarget {};
}

bool DragSession::detachGroup(PanelGroup* group)
{
    DockHost* host = DockCatalog::self().host();
    if (!host || !group || group->vacancy() || group->isCentral())
        return false;

    // 能力门控：不可浮动的分组不拆出为独立窗口
    if (!group->features().testFlag(DockPanel::Feature::Floatable))
        return false;

    // 已在浮窗内（含作为次级分组停靠）不再重复拆出，避免窗口增殖
    if (regionForGroup(group) != host->region())
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
        if (origin) {
            const QList<DockPanel*> docks = group->panels();
            if (docks.isEmpty())
                return false;
            DockPanel* dock = docks.first();

            DockWindow* window = windowForGroup(group);
            // 先归还窗口并销毁临时节点（清除面板拆出标记），再迁移面板
            disposeGroupNode(group, window);

            group->removePanel(dock);
            origin->addPanel(dock);
            origin->setActivePanel(dock);

            floating_origins_.remove(group);
            if (window) {
                // 临时分组是浮窗的 QObject 子对象：先解除父子关系再销毁窗口，
                // 避免本方法在分组自身的信号发射中被调用时连带析构分组
                group->setParent(this);
                closeWindowIfEmpty(window);
            }
            // 延迟销毁临时分组：调用栈上可能仍在分发其信号
            group->deleteLater();

            Q_EMIT layoutChanged();
            return true;
        }
        // 来源分组已被回收（如合并回其他分组）：清除失效记录，走常规回停兜底
        floating_origins_.remove(group);
    }

    DockHost* host = DockCatalog::self().host();
    if (!host)
        return false;

    DockWindow* window = windowForGroup(group);
    if (!window) {
        // 次级分组停靠在浮窗内：按所在区域反查宿主窗口
        if (DockRegion* region = regionForGroup(group)) {
            for (DockWindow* candidate : DockCatalog::self().windows()) {
                if (candidate->region() == region) {
                    window = candidate;
                    break;
                }
            }
        }
    }

    if (window) {
        // 主分组由窗口整树/叶子摘出；次级分组从所在区域摘出
        if (window->group() == group)
            window->releaseGroup(); // 摘出布局节点，浮窗变空
        else if (DockRegion* region = window->region())
            region->extractGroupNode(group);
    }

    if (!host->region()->restoreGroupFromWindow(group)) {
        // 无占位且无回停来源：兜底停靠到主区域，避免分组滞留浮动无法回收
        if (group->node()) {
            host->region()->attachGroup(group, DropZone::OuterRight, nullptr, QSize());
            group->setParent(host->region());
        } else {
            // 无布局节点可兜底：无法停靠，保持现状（分组仍归浮窗）
            return false;
        }
    }

    if (window)
        closeWindowIfEmpty(window);
    Q_EMIT layoutChanged();
    return true;
}

bool DragSession::toggleDetached(PanelGroup* group)
{
    if (!group)
        return false;
    if (isFloating(group))
        return reattachGroup(group);
    return detachGroup(group);
}

bool DragSession::isFloating(PanelGroup* group)
{
    if (!group)
        return false;

    if (group->vacancy() || self().floating_origins_.contains(group))
        return true;

    DockHost* host = DockCatalog::self().host();
    return !host || regionForGroup(group) != host->region();
}

DockWindow* DragSession::createFloatingWindow()
{
    return createDragWindow();
}

void DragSession::destroyFloatingWindow(DockWindow* window)
{
    if (!window)
        return;

    // 布局清理用：先把浮窗内分组全部摘出并回收，再销毁空窗
    if (DockRegion* region = window->region()) {
        const QList<PanelGroup*> groups = region->groups();

        // 摘出的布局节点不再被任何分组引用（~PanelGroup 只清 client），需显式回收
        QList<LayoutNode*> orphan_nodes;
        for (PanelGroup* group : groups) {
            if (region->extractGroupNode(group) && group->node())
                orphan_nodes.append(group->node());
        }

        for (PanelGroup* group : groups) {
            group->setVacancy(nullptr); // 主树占位随后整体回收，避免悬空引用
            const QList<DockPanel*> panels = group->panels();
            for (DockPanel* panel : panels) {
                panel->applyDetached(false);
                panel->applyShown(false);
                group->removePanel(panel);
            }
            floating_origins_.remove(group);
            delete group;
        }

        qDeleteAll(orphan_nodes);
    }

    destroyDragWindow(window);
}

bool DragSession::parkedOrigin(PanelGroup* group, PanelGroup*& origin, int& index) const
{
    if (!group)
        return false;

    const auto it = floating_origins_.constFind(group);
    if (it == floating_origins_.constEnd() || !it->origin_group)
        return false;

    origin = it->origin_group;
    index = it->index;
    return true;
}

void DragSession::restoreParkedGroup(PanelGroup* group, PanelGroup* origin, int index)
{
    if (!group || !origin)
        return;

    floating_origins_.insert(group, FloatOrigin { origin, index });
    connect(group, &QObject::destroyed, this, [this, group] {
        floating_origins_.remove(group);
    });
    refreshFloatingWatcher(windowForGroup(group));
}

void DragSession::startDrag(const QPoint& global_pos)
{
    if (!handle_ || !handle_->group()) {
        cleanup();
        return;
    }

    PanelGroup* source_group = handle_->group();
    DockPanel* dock = handle_->panel();
    // 防御：中央持久分组/面板与不可移动目标不得进入拖拽流程（单标签按面板能力、整组按组能力）
    const bool movable = dock
        ? dock->hasFeature(DockPanel::Feature::Movable)
        : source_group->features().testFlag(DockPanel::Feature::Movable);
    if (source_group->isCentral() || (dock && dock->isCentral()) || !movable) {
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
    drag_proxy_ = new DragProxy(window, global_pos);

    // 拖拽窗口置顶：避免拖动中的浮窗被主窗口遮挡
    if (window && window->view())
        window->view()->bringToFront();

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

    // 保留浮动后置顶，避免浮窗落在主窗口后面被误认为消失
    if (window && window->view())
        window->view()->bringToFront();

    PanelGroup* parked = drag_group_;
    QObject::connect(parked, &QObject::destroyed, this, [this, parked] {
        floating_origins_.remove(parked);
    });
}

void DragSession::updateHover(const QPoint& global_pos)
{
    const DockWindow* dragged_window = drag_proxy_ ? drag_proxy_->window() : nullptr;
    const DockRegion* dragged_region = dragged_window ? dragged_window->region() : nullptr;

    const DropTarget target = DropResolver::resolve(global_pos, dragged_region);
    if (target == hover_)
        return;

    hover_ = target;
    Q_EMIT zoneChanged();
}

bool DragSession::applyDrop()
{
    PanelGroup* group = drag_group_ ? drag_group_ : (handle_ ? handle_->group() : nullptr);
    if (!group || !hover_.region)
        return false;

    DockWindow* window = dragWindow();
    const bool temp_group = drag_group_ && handle_ && drag_group_ != handle_->group();

    if (hover_.zone == DropZone::Merge) {
        if (!hover_.group || hover_.group == group || hover_.group->isCentral())
            return false;

        // 全部面板并入目标分组；悬停标签栏时按插入位置落点
        const QList<DockPanel*> docks = group->panels();
        for (int i = 0; i < docks.size(); ++i) {
            DockPanel* panel = docks.at(i);
            hover_.group->insertPanel(panel, hover_.tab_index < 0 ? -1 : hover_.tab_index + i);
            panel->applyDetached(false);
        }
        for (DockPanel* panel : std::as_const(docks))
            group->removePanel(panel);

        if (source_region_)
            source_region_->discardGroupVacancy(group);

        disposeGroupNode(group, window);

        // 停靠后的分组不再属于浮窗；临时/曾浮出的分组用完即弃，
        // 原分组此时已无面板与节点，同样回收（视图随清空销毁）
        if (temp_group || floating_origins_.contains(group))
            floating_origins_.remove(group);
        group->setParent(this);
        group->deleteLater();
    } else {
        LayoutNode* item = takeGroupNode(group, window);
        if (!item)
            return false;

        if (source_region_)
            source_region_->discardGroupVacancy(group);

        group->setNode(item);
        // 空尺寸 = 无期望尺寸：按公平份额分空间（两项时各占一半）
        hover_.region->attachGroup(group, hover_.zone, hover_.group, QSize());
        // 归纳入目标区域所有：分组从此不再属于浮窗（避免浮窗销毁连带删除）
        group->setParent(hover_.region);
        // 成为分栏：不再需要回停来源
        floating_origins_.remove(group);
    }

    closeWindowIfEmpty(window);
    // 浮窗内分组拓扑可能变化（新增/移除次级分组），重绑空窗监听
    refreshAllFloatingWatchers();
    return true;
}

void DragSession::cleanup()
{
    resetSessionMembers();

    if (phase_ != Phase::Idle) {
        phase_ = Phase::Idle;
        Q_EMIT phaseChanged(phase_);
    }

    if (hover_.region || hover_.zone != DropZone::None) {
        hover_ = DropTarget{};
        Q_EMIT zoneChanged();
    }

    // 兜底：浮窗内面板全部隐藏（含多分组浮窗），会话结束时统一回收
    if (!DockCatalog::self().host())
        return;
    const QList<DockWindow*> windows = DockCatalog::self().windows();
    for (DockWindow* window : windows) {
        if (!window || !window->region())
            continue;

        bool any_shown = false;
        const QList<PanelGroup*> groups = window->region()->groups();
        for (PanelGroup* group : groups) {
            if (!group->shownPanels().isEmpty()) {
                any_shown = true;
                break;
            }
        }
        if (any_shown)
            continue;

        evacuateWindow(window);
    }
}

LayoutNode* DragSession::takeGroupNode(PanelGroup* group, DockWindow* window)
{
    if (!group)
        return nullptr;

    // 主分组在浮窗中：由窗口负责整树/叶子摘出
    if (window && window->group() == group)
        return window->releaseGroup();

    // 节点仍挂在某区域的布局树中（如停靠在浮窗内的次级分组）：先从所属区域摘出，
    // 避免把仍挂在树上的节点再次插入造成一节点两树
    if (DockRegion* region = regionForGroup(group)) {
        if (region->rootNode() == group->node())
            return region->takeRootNode();
        if (region->extractGroupNode(group))
            return group->node();
    }
    return group->node();
}

void DragSession::disposeGroupNode(PanelGroup* group, DockWindow* window)
{
    LayoutNode* item = takeGroupNode(group, window);
    if (!item)
        return;

    // 仍在布局树中的节点需先摘除（含空容器回收），再删除
    if (BoxNode* parent = item->parent()) {
        LayoutNode* result = parent->detachNode(item, true);
        if (result != parent)
            delete parent; // 容器被替代或摘空：原容器已脱离树，需回收
    } else {
        delete item;
    }
    group->setNode(nullptr);
}

DockWindow* DragSession::createDragWindow()
{
    auto* window = new DockWindow();

    DockHost* host = DockCatalog::self().host();
    if (host && host->view()) {
        if (DockView* view = host->view()->createDockWindow(window))
            window->setView(view);
    }

    // 空浮窗自动回停：监听浮窗分组的面板变化
    QObject::connect(window, &DockWindow::groupChanged, this, [this, window] {
        refreshFloatingWatcher(window);
    });
    QObject::connect(window, &QObject::destroyed, this, [this, window] {
        floating_watchers_.remove(window);
    });
    return window;
}

void DragSession::refreshFloatingWatcher(DockWindow* window)
{
    if (!window)
        return;

    const QList<QMetaObject::Connection> previous = floating_watchers_.take(window);
    for (const QMetaObject::Connection& connection : previous)
        QObject::disconnect(connection);

    DockRegion* region = window->region();
    if (!region)
        return;

    const QPointer<DockWindow> guarded_window(window);
    QList<QMetaObject::Connection> bound;
    const QList<PanelGroup*> groups = region->groups();
    for (PanelGroup* group : groups) {
        bound.append(QObject::connect(group, &PanelGroup::panelsChanged, this,
            [this, guarded_window, group] {
                if (phase_ != Phase::Idle || resolving_empty_ || !guarded_window)
                    return;

                DockRegion* current_region = guarded_window->region();
                if (!current_region)
                    return;

                const QList<PanelGroup*> current_groups = current_region->groups();
                if (!current_groups.contains(group) || !group->shownPanels().isEmpty())
                    return;

                bool any_other_shown = false;
                for (PanelGroup* other : current_groups) {
                    if (other != group && !other->shownPanels().isEmpty()) {
                        any_other_shown = true;
                        break;
                    }
                }

                resolving_empty_ = true;
                if (any_other_shown) {
                    // 主分组空出：归还主区域，窗口留给其他分组
                    if (guarded_window->group() == group)
                        reattachGroup(group);
                } else {
                    // 整窗已无可显示面板：逐组回收后关闭
                    evacuateWindow(guarded_window);
                }
                resolving_empty_ = false;
            }));
    }
    floating_watchers_.insert(window, bound);
}

void DragSession::refreshAllFloatingWatchers()
{
    const QList<DockWindow*> windows = DockCatalog::self().windows();
    for (DockWindow* window : windows)
        refreshFloatingWatcher(window);
}

void DragSession::destroyDragWindow(DockWindow* window)
{
    if (!window)
        return;

#ifdef QT_DEBUG
    Q_ASSERT_X(!window->region() || window->region()->groups().isEmpty(),
        "DragSession", "destroying a floating window that still hosts groups");
#endif

    window->close();
    delete window;
}

void DragSession::closeWindowIfEmpty(DockWindow* window)
{
    if (!window)
        return;
    // 仍承载其他分组：保留窗口，主分组离开不影响它们
    if (window->region() && !window->region()->groups().isEmpty())
        return;
    destroyDragWindow(window);
}

void DragSession::evacuateWindow(DockWindow* window)
{
    if (!window)
        return;

    DockHost* host = DockCatalog::self().host();
    if (!host) {
        destroyDragWindow(window);
        return;
    }

    const QPointer<DockWindow> guarded(window);

    // 1) 主分组：有占位/回停来源走标准归还（窗口可能因此清空而销毁）；否则兜底停靠
    PanelGroup* primary = window->group();
    if (primary && (floating_origins_.contains(primary) || primary->vacancy())) {
        reattachGroup(primary);
    } else if (primary) {
        window->releaseGroup();
        host->region()->attachGroup(primary, DropZone::OuterLeft, nullptr, QSize());
        primary->setParent(host->region());
    }
    if (!guarded)
        return;

    // 2) 次级分组（如停靠进浮窗的分组）：摘出节点后兜底停靠主区域
    DockRegion* region = guarded->region();
    if (!region) {
        destroyDragWindow(guarded);
        return;
    }

    const QList<PanelGroup*> rest = region->groups();
    for (PanelGroup* group : rest)
        region->extractGroupNode(group);
    for (PanelGroup* group : rest) {
        host->region()->attachGroup(group, DropZone::OuterRight, nullptr, QSize());
        group->setParent(host->region());
        floating_origins_.remove(group);
    }

    Q_EMIT layoutChanged();
    closeWindowIfEmpty(guarded);
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

    if (DockHost* host = DockCatalog::self().host()) {
        if (host->region()->groups().contains(group))
            return host->region();
    }
    for (DockWindow* window : DockCatalog::self().windows()) {
        if (window->region() && window->region()->groups().contains(group))
            return window->region();
    }
    return nullptr;
}

}
