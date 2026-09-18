/**
 * @file DragController.cpp
 * @brief 拖拽状态机的实现
 */

#include "DragController.h"

#include "Config.h"
#include "DockRegistry.h"
#include "DockWidget.h"
#include "Draggable.h"
#include "DropArea.h"
#include "DropIndicatorOverlay.h"
#include "FloatingWindow.h"
#include "Group.h"
#include "MainWindow.h"
#include "View.h"
#include "WindowBeingDragged.h"
#include "engine/Item.h"
#include "engine/ItemBoxContainer.h"

#include <utility>

namespace dock {

namespace {

//! @brief 布局树中是否包含指定节点
bool treeContains(const Item* root, const Item* item)
{
    if (!root)
        return false;
    if (root == item)
        return true;
    if (!root->isContainer())
        return false;

    const auto* container = static_cast<const ItemBoxContainer*>(root);
    for (Item* child : container->children()) {
        if (treeContains(child, item))
            return true;
    }
    return false;
}

}

DragController& DragController::self()
{
    static DragController controller;
    return controller;
}

DragController::DragController(QObject* parent)
    : QObject(parent)
{
}

DragController::~DragController()
{
    cleanup();
}

FloatingWindow* DragController::draggedFloatingWindow() const
{
    return window_being_dragged_ ? window_being_dragged_->floatingWindow() : nullptr;
}

void DragController::onPress(Draggable* draggable, const QPoint& global_pos)
{
    if (!draggable || !draggable->group())
        return;

    if (state_ != State::Idle)
        cancel();

    state_ = State::Pressed;
    draggable_ = draggable;
    press_pos_ = global_pos;
    Q_EMIT stateChanged(state_);
}

void DragController::onMove(const QPoint& global_pos)
{
    if (state_ == State::Pressed) {
        if ((global_pos - press_pos_).manhattanLength() < Config::kStartDragDistance)
            return;
        startDrag(global_pos);
        return;
    }

    if (state_ == State::Dragging) {
        window_being_dragged_->moveTo(global_pos);
        updateHover(global_pos);
    }
}

void DragController::onRelease(const QPoint& global_pos)
{
    if (state_ != State::Dragging) {
        cleanup();
        return;
    }

    updateHover(global_pos);
    if (hovered_area_ && hovered_location_ != DropLocation_None) {
        applyDrop();
    } else if (drag_group_ && draggable_ && drag_group_ != draggable_->group() && origin_group_) {
        // 未命中落点：单标签浮出保留为浮动窗口，登记回停来源
        parkFloatingGroup(draggedFloatingWindow());
    }

    cleanup();
}

void DragController::cancel()
{
    if (state_ == State::Dragging && drag_group_ && draggable_
        && drag_group_ != draggable_->group()) {
        // 单标签拖拽取消：归还源分组
        FloatingWindow* floating_window = draggedFloatingWindow();
        Group* temp_group = drag_group_;
        DockWidget* dock = dragged_dock_;

        if (floating_window && floating_window->group() == temp_group)
            floating_window->releaseGroup();

        if (dock) {
            temp_group->removeDockWidget(dock);
            if (origin_group_) {
                origin_group_->addDockWidget(dock);
                origin_group_->setCurrentDockWidget(dock);
            }
        }

        Item* item = temp_group->layoutItem();
        delete item;
        temp_group->setLayoutItem(nullptr);
        if (floating_window)
            destroyFloatingWindow(floating_window);

        floating_origins_.remove(temp_group);
        temp_group->deleteLater();
        Q_EMIT layoutChanged();
    } else if (state_ == State::Dragging && draggable_ && draggable_->group()
        && !draggable_->isFloating()) {
        dockGroup(draggable_->group());
    }

    cleanup();
}

bool DragController::floatGroup(Group* group)
{
    MainWindow* main_window = DockRegistry::self().mainWindow();
    if (!main_window || !group || group->placeholderItem())
        return false;

    DropArea* area = main_window->dropArea();
    Item* item = area->takeGroupForFloat(group);
    if (!item)
        return false;

    FloatingWindow* floating_window = createFloatingWindow();
    const QRect target(area->globalOrigin() + item->geometry().topLeft(), item->geometry().size());
    floating_window->takeGroup(group, item);
    floating_window->setGeometry(target);
    Q_EMIT layoutChanged();
    return true;
}

bool DragController::dockGroup(Group* group)
{
    if (!group)
        return false;

    // 单标签浮出：归还源分组（无占位可恢复）
    const auto origin_it = floating_origins_.constFind(group);
    if (origin_it != floating_origins_.constEnd()) {
        Group* origin = origin_it->origin_group;
        if (!origin)
            return false;

        const QList<DockWidget*> docks = group->dockWidgets();
        if (docks.isEmpty())
            return false;
        DockWidget* dock = docks.first();

        FloatingWindow* floating_window = floatingWindowForGroup(group);
        if (floating_window)
            floating_window->releaseGroup();

        group->removeDockWidget(dock);
        origin->addDockWidget(dock);
        origin->setCurrentDockWidget(dock);

        Item* item = group->layoutItem();
        delete item;
        group->setLayoutItem(nullptr);
        floating_origins_.remove(group);
        if (floating_window)
            destroyFloatingWindow(floating_window);

        Q_EMIT layoutChanged();
        return true;
    }

    MainWindow* main_window = DockRegistry::self().mainWindow();
    if (!main_window)
        return false;

    FloatingWindow* floating_window = floatingWindowForGroup(group);
    if (floating_window)
        floating_window->releaseGroup(); // 摘出布局节点，浮窗变空

    const bool restored = main_window->dropArea()->restoreGroupFromFloat(group);
    if (floating_window)
        destroyFloatingWindow(floating_window);
    if (restored)
        Q_EMIT layoutChanged();
    return restored;
}

bool DragController::toggleFloating(Group* group)
{
    if (!group)
        return false;
    if (group->placeholderItem() || floating_origins_.contains(group))
        return dockGroup(group);
    return floatGroup(group);
}

void DragController::startDrag(const QPoint& global_pos)
{
    if (!draggable_ || !draggable_->group()) {
        cleanup();
        return;
    }

    Group* source_group = draggable_->group();
    DockWidget* dock = draggable_->dockWidget();
    const bool single_tab = dock && source_group->openDockWidgets().size() > 1;

    QSize size;
    if (draggable_->floatingWindow())
        size = draggable_->floatingWindow()->geometry().size();
    else if (source_group->layoutItem())
        size = source_group->layoutItem()->geometry().size();
    if (size.isEmpty())
        size = QSize(300, 200);

    DropArea* source_area = areaForGroup(source_group);
    FloatingWindow* floating_window = nullptr;

    if (single_tab) {
        // 从分组中摘出该标签，放入临时分组并浮出
        origin_group_ = source_group;
        origin_index_ = source_group->openDockWidgets().indexOf(dock);
        source_group->removeDockWidget(dock);

        drag_group_ = new Group(this);
        drag_group_->addDockWidget(dock);
        auto* item = new Item(drag_group_);
        drag_group_->setLayoutItem(item);

        floating_window = createFloatingWindow();
        const QPoint window_pos = global_pos - QPoint(size.width() / 2, Config::kTitleBarHeight / 2);
        floating_window->takeGroup(drag_group_, item);
        floating_window->setGeometry(QRect(window_pos, size));
        dragged_dock_ = dock;
    } else {
        drag_group_ = source_group;
        if (draggable_->floatingWindow()) {
            floating_window = draggable_->floatingWindow();
            source_area = floating_window->dropArea();
        } else {
            MainWindow* main_window = DockRegistry::self().mainWindow();
            if (!main_window) {
                cleanup();
                return;
            }

            source_area = main_window->dropArea();
            Item* item = source_area->takeGroupForFloat(source_group);
            if (!item) {
                cleanup();
                return;
            }

            floating_window = createFloatingWindow();
            const QPoint window_pos = global_pos
                - QPoint(size.width() / 2, Config::kTitleBarHeight / 2);
            floating_window->takeGroup(source_group, item);
            floating_window->setGeometry(QRect(window_pos, size));
        }
    }

    source_area_ = source_area;
    window_being_dragged_ = new WindowBeingDragged(draggable_, floating_window, global_pos);

    state_ = State::Dragging;
    Q_EMIT stateChanged(state_);
    updateHover(global_pos);
}

void DragController::parkFloatingGroup(FloatingWindow* floating_window)
{
    if (!floating_window || !drag_group_)
        return;

    const FloatOrigin origin { origin_group_, origin_index_ };
    floating_origins_.insert(drag_group_, origin);
    drag_group_->setParent(floating_window);

    Group* parked = drag_group_;
    QObject::connect(parked, &QObject::destroyed, this, [this, parked] {
        floating_origins_.remove(parked);
    });
}

void DragController::updateHover(const QPoint& global_pos)
{
    DropArea* found_area = nullptr;
    Group* found_group = nullptr;
    DropLocation found_location = DropLocation_None;

    const FloatingWindow* dragged_window = window_being_dragged_
        ? window_being_dragged_->floatingWindow()
        : nullptr;
    const DropArea* dragged_area = dragged_window ? dragged_window->dropArea() : nullptr;

    const auto consider = [&](DropArea* area) -> bool {
        if (!area || area == dragged_area)
            return false;

        const QRect area_rect(area->globalOrigin(), area->geometry().size());

        if (Group* group = area->groupAt(global_pos)) {
            const QRect item_geometry = group->layoutItem()
                ? group->layoutItem()->geometry()
                : QRect();
            const QRect group_rect(area->globalOrigin() + item_geometry.topLeft(),
                item_geometry.size());
            found_area = area;
            found_group = group;
            // 内方框优先；未命中则回退到常显的外方框
            found_location = DropIndicatorOverlay::locationInGroup(group_rect, global_pos);
            if (found_location == DropLocation_None)
                found_location = DropIndicatorOverlay::locationInArea(area_rect, global_pos);
            return true;
        }

        if (!area_rect.contains(global_pos))
            return false;

        // 区域内的任意位置都视为命中该停靠区域：
        // 不在分组上时由外指示器方框决定落点（未对准方框则无落点）
        found_area = area;
        found_group = nullptr;
        found_location = DropIndicatorOverlay::locationInArea(area_rect, global_pos);
        return true;
    };

    const QList<FloatingWindow*>& floating_windows = DockRegistry::self().floatingWindows();
    for (auto it = floating_windows.crbegin(); it != floating_windows.crend() && !found_area; ++it)
        consider((*it)->dropArea());
    if (!found_area) {
        if (MainWindow* main_window = DockRegistry::self().mainWindow())
            consider(main_window->dropArea());
    }

    if (found_area == hovered_area_ && found_group == hovered_group_
        && found_location == hovered_location_)
        return;

    hovered_area_ = found_area;
    hovered_group_ = found_group;
    hovered_location_ = found_location;
    Q_EMIT hoverChanged();
}

void DragController::applyDrop()
{
    Group* group = drag_group_ ? drag_group_ : (draggable_ ? draggable_->group() : nullptr);
    if (!group || !hovered_area_)
        return;

    FloatingWindow* floating_window = draggedFloatingWindow();
    const bool temp_group = drag_group_ && draggable_ && drag_group_ != draggable_->group();

    if (hovered_location_ == DropLocation_Center) {
        if (!hovered_group_ || hovered_group_ == group)
            return;

        // 全部面板并入目标分组
        const QList<DockWidget*> docks = group->dockWidgets();
        for (DockWidget* dock_widget : docks) {
            hovered_group_->addDockWidget(dock_widget);
            dock_widget->markFloating(false);
        }
        for (DockWidget* dock_widget : std::as_const(docks))
            group->removeDockWidget(dock_widget);

        if (source_area_)
            source_area_->removeGroupPlaceholder(group);

        Item* item = floating_window && floating_window->group() == group
            ? floating_window->releaseGroup()
            : group->layoutItem();
        delete item;
        group->setLayoutItem(nullptr);

        if (temp_group) {
            floating_origins_.remove(group);
            group->deleteLater(); // 临时分组：面板已并入目标
        }
    } else {
        if (source_area_)
            source_area_->removeGroupPlaceholder(group);

        Item* item = group->layoutItem();
        if (floating_window && floating_window->group() == group)
            item = floating_window->releaseGroup(); // 从浮窗摘出，所有权在本控制器
        if (item) {
            group->setLayoutItem(item);
            // 空尺寸 = 无期望尺寸：按公平份额分空间（两项时各占一半）
            hovered_area_->attachGroup(group, hovered_location_, hovered_group_, QSize());
            if (temp_group) {
                // 成为分栏：不再需要回停来源
                floating_origins_.remove(group);
                group->setParent(hovered_area_);
            }
        }
    }

    destroyFloatingWindow(floating_window);
}

void DragController::cleanup()
{
    delete window_being_dragged_;
    window_being_dragged_ = nullptr;
    draggable_ = nullptr;
    source_area_ = nullptr;
    dragged_dock_ = nullptr;
    drag_group_ = nullptr;
    origin_group_ = nullptr;
    origin_index_ = -1;

    if (state_ != State::Idle) {
        state_ = State::Idle;
        Q_EMIT stateChanged(state_);
    }

    if (hovered_area_ || hovered_location_ != DropLocation_None) {
        hovered_area_ = nullptr;
        hovered_group_ = nullptr;
        hovered_location_ = DropLocation_None;
        Q_EMIT hoverChanged();
    }
}

FloatingWindow* DragController::createFloatingWindow()
{
    auto* floating_window = new FloatingWindow();

    MainWindow* main_window = DockRegistry::self().mainWindow();
    if (main_window && main_window->view()) {
        if (View* view = main_window->view()->createFloatingWindowView(floating_window))
            floating_window->setView(view);
    }
    return floating_window;
}

void DragController::destroyFloatingWindow(FloatingWindow* floating_window)
{
    if (!floating_window)
        return;

    floating_window->close();
    delete floating_window;
}

FloatingWindow* DragController::floatingWindowForGroup(Group* group)
{
    for (FloatingWindow* floating_window : DockRegistry::self().floatingWindows()) {
        if (floating_window->group() == group)
            return floating_window;
    }
    return nullptr;
}

DropArea* DragController::areaForGroup(Group* group)
{
    if (!group || !group->layoutItem())
        return nullptr;

    const Item* target = group->layoutItem();
    if (MainWindow* main_window = DockRegistry::self().mainWindow()) {
        if (treeContains(main_window->dropArea()->rootItem(), target))
            return main_window->dropArea();
    }
    for (FloatingWindow* floating_window : DockRegistry::self().floatingWindows()) {
        if (treeContains(floating_window->dropArea()->rootItem(), target))
            return floating_window->dropArea();
    }
    return nullptr;
}

}
