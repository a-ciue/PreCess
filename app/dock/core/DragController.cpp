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

#include <utility>

namespace dock {

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
    if (hovered_area_ && hovered_location_ != DropLocation_None)
        applyDrop();
    // 未命中落点时保留浮动状态

    cleanup();
}

void DragController::cancel()
{
    // 源自停靠分组的拖拽被取消：回到主窗口占位处
    if (state_ == State::Dragging && draggable_ && draggable_->group() && !draggable_->isFloating())
        dockGroup(draggable_->group());

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
    MainWindow* main_window = DockRegistry::self().mainWindow();
    if (!main_window || !group)
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
    if (group->placeholderItem())
        return dockGroup(group);
    return floatGroup(group);
}

void DragController::startDrag(const QPoint& global_pos)
{
    if (!draggable_ || !draggable_->group()) {
        cleanup();
        return;
    }

    FloatingWindow* floating_window = draggable_->floatingWindow();
    DropArea* source_area = nullptr;
    QSize size;

    if (floating_window) {
        source_area = floating_window->dropArea();
        size = floating_window->geometry().size();
    } else {
        MainWindow* main_window = DockRegistry::self().mainWindow();
        if (!main_window) {
            cleanup();
            return;
        }

        source_area = main_window->dropArea();
        Item* item = source_area->takeGroupForFloat(draggable_->group());
        if (!item) {
            cleanup();
            return;
        }

        size = item->geometry().size();
        floating_window = createFloatingWindow();
        const QPoint window_pos = global_pos - QPoint(size.width() / 2, Config::kTitleBarHeight / 2);
        floating_window->takeGroup(draggable_->group(), item);
        floating_window->setGeometry(QRect(window_pos, size));
    }

    source_area_ = source_area;
    dragged_size_ = size;
    window_being_dragged_ = new WindowBeingDragged(draggable_, floating_window, global_pos);

    state_ = State::Dragging;
    Q_EMIT stateChanged(state_);
    updateHover(global_pos);
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

        if (Group* group = area->groupAt(global_pos)) {
            const QRect item_geometry = group->layoutItem()
                ? group->layoutItem()->geometry()
                : QRect();
            const QRect group_rect(area->globalOrigin() + item_geometry.topLeft(),
                item_geometry.size());
            found_area = area;
            found_group = group;
            found_location = DropIndicatorOverlay::locationInGroup(group_rect, global_pos);
            return true;
        }

        const QRect area_rect(area->globalOrigin(), area->geometry().size());
        const DropLocation outer = DropIndicatorOverlay::locationInArea(area_rect, global_pos);
        if (outer != DropLocation_None) {
            found_area = area;
            found_group = nullptr;
            found_location = outer;
            return true;
        }
        return false;
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
    Group* group = draggable_ ? draggable_->group() : nullptr;
    if (!group || !hovered_area_)
        return;

    FloatingWindow* floating_window = draggedFloatingWindow();

    if (hovered_location_ == DropLocation_Center) {
        if (!hovered_group_ || hovered_group_ == group)
            return;

        // 全部打开的面板并入目标分组
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
    } else {
        if (source_area_)
            source_area_->removeGroupPlaceholder(group);

        Item* item = group->layoutItem();
        if (floating_window && floating_window->group() == group)
            item = floating_window->releaseGroup(); // 从浮窗摘出，所有权在本控制器
        if (item) {
            group->setLayoutItem(item);
            hovered_area_->attachGroup(group, hovered_location_, hovered_group_, dragged_size_);
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

}
