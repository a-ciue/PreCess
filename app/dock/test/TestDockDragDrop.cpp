/**
 * @file TestDockDragDrop.cpp
 * @brief 拖放核心单元测试：落点计算、指示器几何、浮动/回停、拖拽阈值与停靠
 */

#include "DockTestSupport.h"

#include "core/ClassicIndicators.h"
#include "core/Config.h"
#include "core/DockRegistry.h"
#include "core/DockWidget.h"
#include "core/DragController.h"
#include "core/Draggable.h"
#include "core/DropArea.h"
#include "core/DropIndicatorOverlay.h"
#include "core/FloatingWindow.h"
#include "core/Group.h"
#include "core/MainWindow.h"
#include "engine/Item.h"
#include "engine/SizingInfo.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>

using namespace docktest;

namespace {

//! @brief 在指示器列表中查找指定落点
QRect indicatorRect(const QList<dock::ClassicIndicators::Indicator>& indicators,
    dock::DropLocation location)
{
    for (const auto& indicator : indicators) {
        if (indicator.location == location)
            return indicator.rect;
    }
    return {};
}

}

TEST_CASE("DockDrag: classic drop locations")
{
    const QRect group(100, 100, 400, 300);
    CHECK(dock::DropIndicatorOverlay::locationInGroup(group, QPoint(300, 250)) == dock::DropLocation_Center);
    CHECK(dock::DropIndicatorOverlay::locationInGroup(group, QPoint(110, 250)) == dock::DropLocation_Left);
    CHECK(dock::DropIndicatorOverlay::locationInGroup(group, QPoint(490, 250)) == dock::DropLocation_Right);
    CHECK(dock::DropIndicatorOverlay::locationInGroup(group, QPoint(300, 110)) == dock::DropLocation_Top);
    CHECK(dock::DropIndicatorOverlay::locationInGroup(group, QPoint(300, 390)) == dock::DropLocation_Bottom);
    CHECK(dock::DropIndicatorOverlay::locationInGroup(group, QPoint(50, 50)) == dock::DropLocation_None);

    const QRect area(0, 0, 1000, 800);
    CHECK(dock::DropIndicatorOverlay::locationInArea(area, QPoint(5, 400)) == dock::DropLocation_OutterLeft);
    CHECK(dock::DropIndicatorOverlay::locationInArea(area, QPoint(995, 400)) == dock::DropLocation_OutterRight);
    CHECK(dock::DropIndicatorOverlay::locationInArea(area, QPoint(500, 5)) == dock::DropLocation_OutterTop);
    CHECK(dock::DropIndicatorOverlay::locationInArea(area, QPoint(500, 795)) == dock::DropLocation_OutterBottom);
    CHECK(dock::DropIndicatorOverlay::locationInArea(area, QPoint(500, 400)) == dock::DropLocation_None);
    CHECK(dock::DropIndicatorOverlay::locationInArea(area, QPoint(-5, 400)) == dock::DropLocation_None);
}

TEST_CASE("DockDrag: classic indicator geometry")
{
    const QRect area(0, 0, 1000, 800);
    const QRect group(100, 100, 400, 300);
    const QList<dock::ClassicIndicators::Indicator> indicators
        = dock::ClassicIndicators::indicatorRects(area, group);

    REQUIRE(indicators.size() == 9);
    const QRect center = indicatorRect(indicators, dock::DropLocation_Center);
    CHECK(center.center() == group.center());

    const QRect left = indicatorRect(indicators, dock::DropLocation_Left);
    CHECK(left.center().x() < center.center().x());
    CHECK(left.center().y() == center.center().y());

    const QRect outer_left = indicatorRect(indicators, dock::DropLocation_OutterLeft);
    CHECK(outer_left.left() == area.left() + dock::Config::kIndicatorMargin);
    CHECK(outer_left.center().y() == area.center().y());

    const QRect outer_bottom = indicatorRect(indicators, dock::DropLocation_OutterBottom);
    CHECK(outer_bottom.bottom() == area.bottom() - dock::Config::kIndicatorMargin);
    CHECK(outer_bottom.center().x() == area.center().x());
}

TEST_CASE("DockDrag: float and dock back restores placeholder position")
{
    DockFixture f;
    PlacedDocks placed(f);
    placed.console->open();

    const QRect console_geometry = placed.console->group()->layoutItem()->geometry();
    dock::DragController& drag = dock::DragController::self();

    REQUIRE(drag.floatGroup(placed.console->group()));
    CHECK(placed.console->isFloating());
    CHECK(placed.console->isOpen());
    REQUIRE(placed.console->group()->placeholderItem() != nullptr);
    CHECK(placed.console->group()->placeholderItem()->parent() != nullptr);
    CHECK_FALSE(placed.console->group()->placeholderItem()->isVisible());

    REQUIRE(dock::DockRegistry::self().floatingWindows().size() == 1);
    dock::FloatingWindow* floating_window = dock::DockRegistry::self().floatingWindows().first();
    CHECK(floating_window->group() == placed.console->group());
    CHECK(floating_window->dropArea()->rootItem() == placed.console->group()->layoutItem());

    REQUIRE(drag.dockGroup(placed.console->group()));
    CHECK_FALSE(placed.console->isFloating());
    CHECK(placed.console->group()->placeholderItem() == nullptr);
    CHECK(placed.console->group()->layoutItem()->isVisible());
    CHECK(dock::DockRegistry::self().floatingWindows().isEmpty());

    const QRect restored = placed.console->group()->layoutItem()->geometry();
    const int height_difference = std::abs(restored.height() - console_geometry.height());
    CHECK(height_difference <= 2);
}

TEST_CASE("DockDrag: drag threshold then cancel restores docked group")
{
    DockFixture f;
    PlacedDocks placed(f);
    placed.console->open();

    dock::DragController& drag = dock::DragController::self();
    dock::Draggable draggable(nullptr, placed.console->group());

    drag.onPress(&draggable, QPoint(200, 200));
    CHECK(drag.state() == dock::DragController::State::Pressed);

    drag.onMove(QPoint(202, 200));
    CHECK(drag.state() == dock::DragController::State::Pressed);

    drag.onMove(QPoint(220, 200));
    CHECK(drag.state() == dock::DragController::State::Dragging);
    CHECK(placed.console->isFloating());

    drag.cancel();
    CHECK(drag.state() == dock::DragController::State::Idle);
    CHECK_FALSE(placed.console->isFloating());
    CHECK(placed.console->group()->placeholderItem() == nullptr);
    CHECK(dock::DockRegistry::self().floatingWindows().isEmpty());
}

TEST_CASE("DockDrag: dropping on group center merges tabs")
{
    DockFixture f;
    PlacedDocks placed(f);
    placed.console->open();

    dock::DragController& drag = dock::DragController::self();
    REQUIRE(drag.floatGroup(placed.console->group()));

    dock::FloatingWindow* floating_window = dock::DockRegistry::self().floatingWindows().first();
    dock::Draggable draggable(nullptr, placed.console->group(), floating_window);

    const QRect target_rect = placed.object_tree->group()->layoutItem()->geometry();
    const QPoint target_center = target_rect.center();

    drag.onPress(&draggable, target_center + QPoint(-50, -50));
    drag.onMove(target_center + QPoint(-40, -40));
    REQUIRE(drag.state() == dock::DragController::State::Dragging);
    CHECK(drag.hoveredLocation() == dock::DropLocation_Center);

    drag.onRelease(target_center);
    CHECK(drag.state() == dock::DragController::State::Idle);
    CHECK(placed.console->group() == placed.object_tree->group());
    CHECK_FALSE(placed.console->isFloating());
    CHECK(placed.console->isOpen());
    CHECK(placed.object_tree->group()->dockWidgets().contains(placed.console));
    CHECK(dock::DockRegistry::self().floatingWindows().isEmpty());
}

TEST_CASE("DockDrag: dropping on group left docks to the side")
{
    DockFixture f;
    PlacedDocks placed(f);
    placed.console->open();

    dock::DragController& drag = dock::DragController::self();
    REQUIRE(drag.floatGroup(placed.console->group()));

    dock::FloatingWindow* floating_window = dock::DockRegistry::self().floatingWindows().first();
    dock::Draggable draggable(nullptr, placed.console->group(), floating_window);

    const QRect target_rect = placed.object_tree->group()->layoutItem()->geometry();
    const int center_y = target_rect.center().y();
    const QPoint press_point(target_rect.left() + 30, center_y);
    const QPoint drop_point(target_rect.left() + 10, center_y);

    drag.onPress(&draggable, press_point);
    drag.onMove(drop_point);
    REQUIRE(drag.state() == dock::DragController::State::Dragging);
    CHECK(drag.hoveredLocation() == dock::DropLocation_Left);
    CHECK(drag.hoveredGroup() == placed.object_tree->group());

    drag.onRelease(drop_point);

    CHECK_FALSE(placed.console->isFloating());
    const dock::Item* console_item = placed.console->group()->layoutItem();
    REQUIRE(console_item != nullptr);
    CHECK(console_item->isVisible());
    CHECK(console_item->geometry().x() == 0);
    CHECK(console_item->geometry().width() > 0);

    const dock::Item* object_tree_item = placed.object_tree->group()->layoutItem();
    REQUIRE(object_tree_item != nullptr);
    CHECK(object_tree_item->geometry().x() == console_item->geometry().width() + dock::kSeparatorThickness);
    CHECK(dock::DockRegistry::self().floatingWindows().isEmpty());
}
