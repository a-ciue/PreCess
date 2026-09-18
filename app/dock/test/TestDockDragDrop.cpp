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
    const QPoint center = group.center();

    // 命中方框中心 → 对应落点（方框与绘制一致）
    CHECK(dock::DropIndicatorOverlay::locationInGroup(group, center) == dock::DropLocation_Center);
    CHECK(dock::DropIndicatorOverlay::locationInGroup(group, center + QPoint(-50, 0)) == dock::DropLocation_Left);
    CHECK(dock::DropIndicatorOverlay::locationInGroup(group, center + QPoint(50, 0)) == dock::DropLocation_Right);
    CHECK(dock::DropIndicatorOverlay::locationInGroup(group, center + QPoint(0, -50)) == dock::DropLocation_Top);
    CHECK(dock::DropIndicatorOverlay::locationInGroup(group, center + QPoint(0, 50)) == dock::DropLocation_Bottom);

    // 分组内但未对准任何方框、以及分组外 → 无落点
    CHECK(dock::DropIndicatorOverlay::locationInGroup(group, QPoint(group.left() + 10, center.y())) == dock::DropLocation_None);
    CHECK(dock::DropIndicatorOverlay::locationInGroup(group, QPoint(50, 50)) == dock::DropLocation_None);

    const QRect area(0, 0, 1000, 800);
    const QPoint area_center = area.center();
    // 外方框中心命中
    CHECK(dock::DropIndicatorOverlay::locationInArea(area, QPoint(area.left() + 29, area_center.y())) == dock::DropLocation_OutterLeft);
    CHECK(dock::DropIndicatorOverlay::locationInArea(area, QPoint(area.right() - 30, area_center.y())) == dock::DropLocation_OutterRight);
    CHECK(dock::DropIndicatorOverlay::locationInArea(area, QPoint(area_center.x(), area.top() + 29)) == dock::DropLocation_OutterTop);
    CHECK(dock::DropIndicatorOverlay::locationInArea(area, QPoint(area_center.x(), area.bottom() - 30)) == dock::DropLocation_OutterBottom);
    // 区域中心与区域外 → 无落点
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
    drag.onMove(target_center);
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
    const int original_width = target_rect.width();
    const QPoint target_center = target_rect.center();
    const QPoint press_point(target_center);
    const QPoint drop_point(target_center + QPoint(-50, 0)); // 左指示器方框中心

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
    // 公平份额：两项时新面板约占目标区域一半（取整误差 ±3px）
    const int console_width = console_item->geometry().width();
    CHECK(std::abs(console_width - original_width / 2) <= 3);

    const dock::Item* object_tree_item = placed.object_tree->group()->layoutItem();
    REQUIRE(object_tree_item != nullptr);
    CHECK(console_width + dock::kSeparatorThickness + object_tree_item->geometry().width()
        == original_width);
    CHECK(object_tree_item->geometry().x() == console_width + dock::kSeparatorThickness);
    CHECK(dock::DockRegistry::self().floatingWindows().isEmpty());
}

TEST_CASE("DockDrag: releasing away from indicator boxes keeps floating")
{
    DockFixture f;
    PlacedDocks placed(f);
    placed.console->open();

    dock::DragController& drag = dock::DragController::self();
    REQUIRE(drag.floatGroup(placed.console->group()));
    REQUIRE(dock::DockRegistry::self().floatingWindows().size() == 1);

    dock::FloatingWindow* floating_window = dock::DockRegistry::self().floatingWindows().first();
    dock::Draggable draggable(nullptr, placed.console->group(), floating_window);

    // 分组内但远离指示器方框：无落点
    const QRect target_rect = placed.object_tree->group()->layoutItem()->geometry();
    const QPoint off_box(target_rect.left() + 20, target_rect.center().y());

    drag.onPress(&draggable, off_box + QPoint(0, -60));
    drag.onMove(off_box);
    REQUIRE(drag.state() == dock::DragController::State::Dragging);
    CHECK(drag.hoveredGroup() == placed.object_tree->group());
    CHECK(drag.hoveredLocation() == dock::DropLocation_None);

    drag.onRelease(off_box);
    CHECK(drag.state() == dock::DragController::State::Idle);
    CHECK(placed.console->isFloating());
    CHECK(dock::DockRegistry::self().floatingWindows().size() == 1);

    // 清理：回停到占位处
    REQUIRE(drag.dockGroup(placed.console->group()));
    CHECK_FALSE(placed.console->isFloating());
    CHECK(dock::DockRegistry::self().floatingWindows().isEmpty());
}

TEST_CASE("DockDrag: hovering a gap still targets the dock area")
{
    DockFixture f;
    PlacedDocks placed(f);
    placed.console->open();

    dock::DragController& drag = dock::DragController::self();
    REQUIRE(drag.floatGroup(placed.console->group()));
    REQUIRE(dock::DockRegistry::self().floatingWindows().size() == 1);

    dock::FloatingWindow* floating_window = dock::DockRegistry::self().floatingWindows().first();
    dock::Draggable draggable(nullptr, placed.console->group(), floating_window);

    // 分隔条处（不属于任何分组，但位于停靠区域内）
    const dock::Item* object_tree_item = placed.object_tree->group()->layoutItem();
    REQUIRE(object_tree_item != nullptr);
    const QRect item_geometry = object_tree_item->geometry();
    const QPoint gap(item_geometry.right() + 1 + dock::kSeparatorThickness / 2,
        item_geometry.top() + 100);

    drag.onPress(&draggable, gap + QPoint(0, 60));
    drag.onMove(gap);
    REQUIRE(drag.state() == dock::DragController::State::Dragging);
    CHECK(drag.hoveredArea() == f.main_window.dropArea());
    CHECK(drag.hoveredGroup() == nullptr);
    CHECK(drag.hoveredLocation() == dock::DropLocation_None);

    // 未对准外方框释放：保持浮动
    drag.onRelease(gap);
    CHECK(drag.state() == dock::DragController::State::Idle);
    CHECK(placed.console->isFloating());
    CHECK(dock::DockRegistry::self().floatingWindows().size() == 1);

    // 清理：回停到占位处
    REQUIRE(drag.dockGroup(placed.console->group()));
    CHECK_FALSE(placed.console->isFloating());
    CHECK(dock::DockRegistry::self().floatingWindows().isEmpty());
}

TEST_CASE("DockDrag: outer indicator boxes stay reachable over a group")
{
    DockFixture f;
    PlacedDocks placed(f);
    placed.console->open();

    dock::DragController& drag = dock::DragController::self();
    REQUIRE(drag.floatGroup(placed.console->group()));
    REQUIRE(dock::DockRegistry::self().floatingWindows().size() == 1);

    dock::FloatingWindow* floating_window = dock::DockRegistry::self().floatingWindows().first();
    dock::Draggable draggable(nullptr, placed.console->group(), floating_window);

    // 主窗口底边外方框中心：该位置同时被中央面板覆盖（分组内）
    const dock::DropArea* area = f.main_window.dropArea();
    const QRect area_rect(area->globalOrigin(), area->geometry().size());
    QPoint drop_point;
    for (const dock::ClassicIndicators::Indicator& indicator
        : dock::ClassicIndicators::outerIndicatorRects(area_rect)) {
        if (indicator.location == dock::DropLocation_OutterBottom)
            drop_point = indicator.rect.center();
    }
    REQUIRE(drop_point != QPoint());

    drag.onPress(&draggable, area_rect.center());
    drag.onMove(drop_point);
    REQUIRE(drag.state() == dock::DragController::State::Dragging);
    CHECK(drag.hoveredGroup() == f.main_window.centralGroup()); // 光标确实在面板上
    CHECK(drag.hoveredLocation() == dock::DropLocation_OutterBottom); // 外框仍可命中

    drag.onRelease(drop_point);
    CHECK(drag.state() == dock::DragController::State::Idle);
    CHECK_FALSE(placed.console->isFloating());
    CHECK(dock::DockRegistry::self().floatingWindows().isEmpty());

    const dock::Item* console_item = placed.console->group()->layoutItem();
    REQUIRE(console_item != nullptr);
    CHECK(console_item->isVisible());
    // 外落点同样按公平份额：两项时约占一半
    CHECK(std::abs(console_item->geometry().height() - area_rect.height() / 2) <= 3);
}

TEST_CASE("DockDrag: dragging a tab separates one panel and can return")
{
    DockFixture f;
    PlacedDocks placed(f);
    placed.object_tree->open();
    placed.console->open();

    dock::DragController& drag = dock::DragController::self();

    // 先把 console 合并到 objectTree 分组（中心落点）
    REQUIRE(drag.floatGroup(placed.console->group()));
    {
        dock::FloatingWindow* floating_window = dock::DockRegistry::self().floatingWindows().first();
        dock::Draggable draggable(nullptr, placed.console->group(), floating_window);
        const QRect target = placed.object_tree->group()->layoutItem()->geometry();
        const QPoint center = target.center();
        drag.onPress(&draggable, center + QPoint(-50, -50));
        drag.onMove(center);
        REQUIRE(drag.state() == dock::DragController::State::Dragging);
        drag.onRelease(center);
    }
    REQUIRE(placed.console->group() == placed.object_tree->group());
    REQUIRE(placed.object_tree->group()->openDockWidgets().size() == 2);

    const auto tab_press_point = [&] {
        return placed.object_tree->group()->layoutItem()->geometry().center();
    };

    // 拖出 console 标签：只分离该面板，源分组保留其余标签
    dock::Draggable tab_draggable(nullptr, placed.object_tree->group(), nullptr, placed.console);
    drag.onPress(&tab_draggable, tab_press_point());
    drag.onMove(tab_press_point() + QPoint(60, 0));
    REQUIRE(drag.state() == dock::DragController::State::Dragging);
    REQUIRE(dock::DockRegistry::self().floatingWindows().size() == 1);
    {
        dock::Group* floating_group = dock::DockRegistry::self().floatingWindows().first()->group();
        REQUIRE(floating_group != nullptr);
        CHECK(floating_group->dockWidgets().contains(placed.console));
        CHECK_FALSE(floating_group->dockWidgets().contains(placed.object_tree));
    }
    CHECK(placed.console->isFloating());
    CHECK(placed.object_tree->group()->openDockWidgets().size() == 1);
    CHECK(placed.object_tree->group()->openDockWidgets().contains(placed.object_tree));

    // 取消：归还源分组
    drag.cancel();
    CHECK(drag.state() == dock::DragController::State::Idle);
    CHECK(dock::DockRegistry::self().floatingWindows().isEmpty());
    CHECK(placed.console->group() == placed.object_tree->group());
    CHECK(placed.object_tree->group()->openDockWidgets().size() == 2);
    CHECK_FALSE(placed.console->isFloating());

    // 再次拖出到空白处：保留浮动，回停按钮可归还源分组
    dock::Draggable tab_draggable2(nullptr, placed.object_tree->group(), nullptr, placed.console);
    drag.onPress(&tab_draggable2, tab_press_point());
    drag.onMove(tab_press_point() + QPoint(60, 0));
    REQUIRE(drag.state() == dock::DragController::State::Dragging);
    drag.onRelease(QPoint(-1000, -1000));
    CHECK(drag.state() == dock::DragController::State::Idle);
    REQUIRE(dock::DockRegistry::self().floatingWindows().size() == 1);
    {
        dock::FloatingWindow* floating_window = dock::DockRegistry::self().floatingWindows().first();
        REQUIRE(floating_window->group() != nullptr);
        CHECK(floating_window->group()->dockWidgets().contains(placed.console));
        REQUIRE(drag.dockGroup(floating_window->group()));
    }
    CHECK(placed.console->group() == placed.object_tree->group());
    CHECK(dock::DockRegistry::self().floatingWindows().isEmpty());
    CHECK_FALSE(placed.console->isFloating());
}
