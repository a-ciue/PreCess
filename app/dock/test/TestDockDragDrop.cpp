/**
 * @file TestDockDragDrop.cpp
 * @brief 拖放核心单元测试：落点计算、指示器几何、浮动/回停、拖拽阈值与停靠
 */

#include "DockTestSupport.h"

#include "docking/ZoneGeometry.h"
#include "docking/DockMetrics.h"
#include "docking/DockCatalog.h"
#include "docking/DockPanel.h"
#include "docking/DragSession.h"
#include "docking/DragHandle.h"
#include "docking/DockRegion.h"
#include "docking/ZoneResolver.h"
#include "docking/DockWindow.h"
#include "docking/PanelGroup.h"
#include "docking/DockHost.h"
#include "tree/LayoutNode.h"
#include "tree/NodeMetrics.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>

using namespace docktest;

namespace {

//! @brief 在指示器列表中查找指定落点
QRect indicatorRect(const QList<dock::ZoneGeometry::ZoneRect>& zones,
    dock::DropZone location)
{
    for (const auto& indicator : zones) {
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
    CHECK(dock::ZoneResolver::zoneInGroup(group, center) == dock::DropZone::Merge);
    CHECK(dock::ZoneResolver::zoneInGroup(group, center + QPoint(-50, 0)) == dock::DropZone::InnerLeft);
    CHECK(dock::ZoneResolver::zoneInGroup(group, center + QPoint(50, 0)) == dock::DropZone::InnerRight);
    CHECK(dock::ZoneResolver::zoneInGroup(group, center + QPoint(0, -50)) == dock::DropZone::InnerTop);
    CHECK(dock::ZoneResolver::zoneInGroup(group, center + QPoint(0, 50)) == dock::DropZone::InnerBottom);

    // 分组内但未对准任何方框、以及分组外 → 无落点
    CHECK(dock::ZoneResolver::zoneInGroup(group, QPoint(group.left() + 10, center.y())) == dock::DropZone::None);
    CHECK(dock::ZoneResolver::zoneInGroup(group, QPoint(50, 50)) == dock::DropZone::None);

    const QRect area(0, 0, 1000, 800);
    const QPoint area_center = area.center();
    // 外方框中心命中
    CHECK(dock::ZoneResolver::zoneInRegion(area, QPoint(area.left() + 29, area_center.y())) == dock::DropZone::OuterLeft);
    CHECK(dock::ZoneResolver::zoneInRegion(area, QPoint(area.right() - 30, area_center.y())) == dock::DropZone::OuterRight);
    CHECK(dock::ZoneResolver::zoneInRegion(area, QPoint(area_center.x(), area.top() + 29)) == dock::DropZone::OuterTop);
    CHECK(dock::ZoneResolver::zoneInRegion(area, QPoint(area_center.x(), area.bottom() - 30)) == dock::DropZone::OuterBottom);
    // 区域中心与区域外 → 无落点
    CHECK(dock::ZoneResolver::zoneInRegion(area, QPoint(500, 400)) == dock::DropZone::None);
    CHECK(dock::ZoneResolver::zoneInRegion(area, QPoint(-5, 400)) == dock::DropZone::None);
}

TEST_CASE("DockDrag: classic indicator geometry")
{
    const QRect area(0, 0, 1000, 800);
    const QRect group(100, 100, 400, 300);
    const QList<dock::ZoneGeometry::ZoneRect> zones
        = dock::ZoneGeometry::allZones(area, group);

    REQUIRE(zones.size() == 9);
    const QRect center = indicatorRect(zones, dock::DropZone::Merge);
    CHECK(center.center() == group.center());

    const QRect left = indicatorRect(zones, dock::DropZone::InnerLeft);
    CHECK(left.center().x() < center.center().x());
    CHECK(left.center().y() == center.center().y());

    const QRect outer_left = indicatorRect(zones, dock::DropZone::OuterLeft);
    CHECK(outer_left.left() == area.left() + dock::DockMetrics::kZoneMargin);
    CHECK(outer_left.center().y() == area.center().y());

    const QRect outer_bottom = indicatorRect(zones, dock::DropZone::OuterBottom);
    CHECK(outer_bottom.bottom() == area.bottom() - dock::DockMetrics::kZoneMargin);
    CHECK(outer_bottom.center().x() == area.center().x());
}

TEST_CASE("DockDrag: float and dock back restores placeholder position")
{
    DockFixture f;
    PlacedDocks placed(f);
    placed.console->showPanel();

    const QRect console_geometry = placed.console->group()->node()->geometry();
    dock::DragSession& drag = dock::DragSession::self();

    REQUIRE(drag.detachGroup(placed.console->group()));
    CHECK(placed.console->isDetached());
    CHECK(placed.console->isShown());
    REQUIRE(placed.console->group()->vacancy() != nullptr);
    CHECK(placed.console->group()->vacancy()->parent() != nullptr);
    CHECK_FALSE(placed.console->group()->vacancy()->isVisible());

    REQUIRE(dock::DockCatalog::self().windows().size() == 1);
    dock::DockWindow* window = dock::DockCatalog::self().windows().first();
    CHECK(window->group() == placed.console->group());
    CHECK(window->region()->rootNode() == placed.console->group()->node());

    REQUIRE(drag.reattachGroup(placed.console->group()));
    CHECK_FALSE(placed.console->isDetached());
    CHECK(placed.console->group()->vacancy() == nullptr);
    CHECK(placed.console->group()->node()->isVisible());
    CHECK(dock::DockCatalog::self().windows().isEmpty());

    const QRect restored = placed.console->group()->node()->geometry();
    const int height_difference = std::abs(restored.height() - console_geometry.height());
    CHECK(height_difference <= 2);
}

TEST_CASE("DockDrag: drag threshold then cancel restores docked group")
{
    DockFixture f;
    PlacedDocks placed(f);
    placed.console->showPanel();

    dock::DragSession& drag = dock::DragSession::self();
    dock::DragHandle handle(nullptr, placed.console->group());

    drag.beginAt(&handle, QPoint(200, 200));
    CHECK(drag.phase() == dock::DragSession::Phase::Armed);

    drag.updateAt(QPoint(202, 200));
    CHECK(drag.phase() == dock::DragSession::Phase::Armed);

    drag.updateAt(QPoint(220, 200));
    CHECK(drag.phase() == dock::DragSession::Phase::Dragging);
    CHECK(placed.console->isDetached());

    drag.cancel();
    CHECK(drag.phase() == dock::DragSession::Phase::Idle);
    CHECK_FALSE(placed.console->isDetached());
    CHECK(placed.console->group()->vacancy() == nullptr);
    CHECK(dock::DockCatalog::self().windows().isEmpty());
}

TEST_CASE("DockDrag: dropping on group center merges tabs")
{
    DockFixture f;
    PlacedDocks placed(f);
    placed.console->showPanel();

    dock::DragSession& drag = dock::DragSession::self();
    REQUIRE(drag.detachGroup(placed.console->group()));

    dock::DockWindow* window = dock::DockCatalog::self().windows().first();
    dock::DragHandle handle(nullptr, placed.console->group(), window);

    const QRect target_rect = placed.object_tree->group()->node()->geometry();
    const QPoint target_center = target_rect.center();

    drag.beginAt(&handle, target_center + QPoint(-50, -50));
    drag.updateAt(target_center);
    REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);
    CHECK(drag.hoveredZone() == dock::DropZone::Merge);

    drag.endAt(target_center);
    CHECK(drag.phase() == dock::DragSession::Phase::Idle);
    CHECK(placed.console->group() == placed.object_tree->group());
    CHECK_FALSE(placed.console->isDetached());
    CHECK(placed.console->isShown());
    CHECK(placed.object_tree->group()->panels().contains(placed.console));
    CHECK(dock::DockCatalog::self().windows().isEmpty());
}

TEST_CASE("DockDrag: dropping on group left docks to the side")
{
    DockFixture f;
    PlacedDocks placed(f);
    placed.console->showPanel();

    dock::DragSession& drag = dock::DragSession::self();
    REQUIRE(drag.detachGroup(placed.console->group()));

    dock::DockWindow* window = dock::DockCatalog::self().windows().first();
    dock::DragHandle handle(nullptr, placed.console->group(), window);

    const QRect target_rect = placed.object_tree->group()->node()->geometry();
    const int original_width = target_rect.width();
    const QPoint target_center = target_rect.center();
    const QPoint press_point(target_center);
    const QPoint drop_point(target_center + QPoint(-50, 0)); // 左指示器方框中心

    drag.beginAt(&handle, press_point);
    drag.updateAt(drop_point);
    REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);
    CHECK(drag.hoveredZone() == dock::DropZone::InnerLeft);
    CHECK(drag.hoveredGroup() == placed.object_tree->group());

    drag.endAt(drop_point);

    CHECK_FALSE(placed.console->isDetached());
    const dock::LayoutNode* console_item = placed.console->group()->node();
    REQUIRE(console_item != nullptr);
    CHECK(console_item->isVisible());
    CHECK(console_item->geometry().x() == 0);
    // 公平份额：两项时新面板约占目标区域一半（取整误差 ±3px）
    const int console_width = console_item->geometry().width();
    CHECK(std::abs(console_width - original_width / 2) <= 3);

    const dock::LayoutNode* object_tree_item = placed.object_tree->group()->node();
    REQUIRE(object_tree_item != nullptr);
    CHECK(console_width + dock::kDividerThickness + object_tree_item->geometry().width()
        == original_width);
    CHECK(object_tree_item->geometry().x() == console_width + dock::kDividerThickness);
    CHECK(dock::DockCatalog::self().windows().isEmpty());
}

TEST_CASE("DockDrag: releasing away from indicator boxes keeps floating")
{
    DockFixture f;
    PlacedDocks placed(f);
    placed.console->showPanel();

    dock::DragSession& drag = dock::DragSession::self();
    REQUIRE(drag.detachGroup(placed.console->group()));
    REQUIRE(dock::DockCatalog::self().windows().size() == 1);

    dock::DockWindow* window = dock::DockCatalog::self().windows().first();
    dock::DragHandle handle(nullptr, placed.console->group(), window);

    // 分组内但远离指示器方框：无落点
    const QRect target_rect = placed.object_tree->group()->node()->geometry();
    const QPoint off_box(target_rect.left() + 20, target_rect.center().y());

    drag.beginAt(&handle, off_box + QPoint(0, -60));
    drag.updateAt(off_box);
    REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);
    CHECK(drag.hoveredGroup() == placed.object_tree->group());
    CHECK(drag.hoveredZone() == dock::DropZone::None);

    drag.endAt(off_box);
    CHECK(drag.phase() == dock::DragSession::Phase::Idle);
    CHECK(placed.console->isDetached());
    CHECK(dock::DockCatalog::self().windows().size() == 1);

    // 清理：回停到占位处
    REQUIRE(drag.reattachGroup(placed.console->group()));
    CHECK_FALSE(placed.console->isDetached());
    CHECK(dock::DockCatalog::self().windows().isEmpty());
}

TEST_CASE("DockDrag: hovering a gap still targets the dock area")
{
    DockFixture f;
    PlacedDocks placed(f);
    placed.console->showPanel();

    dock::DragSession& drag = dock::DragSession::self();
    REQUIRE(drag.detachGroup(placed.console->group()));
    REQUIRE(dock::DockCatalog::self().windows().size() == 1);

    dock::DockWindow* window = dock::DockCatalog::self().windows().first();
    dock::DragHandle handle(nullptr, placed.console->group(), window);

    // 分隔条处（不属于任何分组，但位于停靠区域内）
    const dock::LayoutNode* object_tree_item = placed.object_tree->group()->node();
    REQUIRE(object_tree_item != nullptr);
    const QRect item_geometry = object_tree_item->geometry();
    const QPoint gap(item_geometry.right() + 1 + dock::kDividerThickness / 2,
        item_geometry.top() + 100);

    drag.beginAt(&handle, gap + QPoint(0, 60));
    drag.updateAt(gap);
    REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);
    CHECK(drag.hoveredRegion() == f.host.region());
    CHECK(drag.hoveredGroup() == nullptr);
    CHECK(drag.hoveredZone() == dock::DropZone::None);

    // 未对准外方框释放：保持浮动
    drag.endAt(gap);
    CHECK(drag.phase() == dock::DragSession::Phase::Idle);
    CHECK(placed.console->isDetached());
    CHECK(dock::DockCatalog::self().windows().size() == 1);

    // 清理：回停到占位处
    REQUIRE(drag.reattachGroup(placed.console->group()));
    CHECK_FALSE(placed.console->isDetached());
    CHECK(dock::DockCatalog::self().windows().isEmpty());
}

TEST_CASE("DockDrag: outer indicator boxes stay reachable over a group")
{
    DockFixture f;
    PlacedDocks placed(f);
    placed.console->showPanel();

    dock::DragSession& drag = dock::DragSession::self();
    REQUIRE(drag.detachGroup(placed.console->group()));
    REQUIRE(dock::DockCatalog::self().windows().size() == 1);

    dock::DockWindow* window = dock::DockCatalog::self().windows().first();
    dock::DragHandle handle(nullptr, placed.console->group(), window);

    // 主窗口底边外方框中心：该位置同时被中央面板覆盖（分组内）
    const dock::DockRegion* area = f.host.region();
    const QRect area_rect(area->globalOrigin(), area->geometry().size());
    QPoint drop_point;
    for (const dock::ZoneGeometry::ZoneRect& indicator
        : dock::ZoneGeometry::outerZones(area_rect)) {
        if (indicator.location == dock::DropZone::OuterBottom)
            drop_point = indicator.rect.center();
    }
    REQUIRE(drop_point != QPoint());

    drag.beginAt(&handle, area_rect.center());
    drag.updateAt(drop_point);
    REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);
    CHECK(drag.hoveredGroup() == f.host.centralGroup()); // 光标确实在面板上
    CHECK(drag.hoveredZone() == dock::DropZone::OuterBottom); // 外框仍可命中

    drag.endAt(drop_point);
    CHECK(drag.phase() == dock::DragSession::Phase::Idle);
    CHECK_FALSE(placed.console->isDetached());
    CHECK(dock::DockCatalog::self().windows().isEmpty());

    const dock::LayoutNode* console_item = placed.console->group()->node();
    REQUIRE(console_item != nullptr);
    CHECK(console_item->isVisible());
    // 外落点同样按公平份额：两项时约占一半
    CHECK(std::abs(console_item->geometry().height() - area_rect.height() / 2) <= 3);
}

TEST_CASE("DockDrag: dragging a tab separates one panel and can return")
{
    DockFixture f;
    PlacedDocks placed(f);
    placed.object_tree->showPanel();
    placed.console->showPanel();

    dock::DragSession& drag = dock::DragSession::self();

    // 先把 console 合并到 objectTree 分组（中心落点）
    REQUIRE(drag.detachGroup(placed.console->group()));
    {
        dock::DockWindow* window = dock::DockCatalog::self().windows().first();
        dock::DragHandle handle(nullptr, placed.console->group(), window);
        const QRect target = placed.object_tree->group()->node()->geometry();
        const QPoint center = target.center();
        drag.beginAt(&handle, center + QPoint(-50, -50));
        drag.updateAt(center);
        REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);
        drag.endAt(center);
    }
    REQUIRE(placed.console->group() == placed.object_tree->group());
    REQUIRE(placed.object_tree->group()->shownPanels().size() == 2);

    const auto tab_press_point = [&] {
        return placed.object_tree->group()->node()->geometry().center();
    };

    // 拖出 console 标签：只分离该面板，源分组保留其余标签
    dock::DragHandle tab_handle(nullptr, placed.object_tree->group(), nullptr, placed.console);
    drag.beginAt(&tab_handle, tab_press_point());
    drag.updateAt(tab_press_point() + QPoint(60, 0));
    REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);
    REQUIRE(dock::DockCatalog::self().windows().size() == 1);
    {
        dock::PanelGroup* floating_group = dock::DockCatalog::self().windows().first()->group();
        REQUIRE(floating_group != nullptr);
        CHECK(floating_group->panels().contains(placed.console));
        CHECK_FALSE(floating_group->panels().contains(placed.object_tree));
    }
    CHECK(placed.console->isDetached());
    CHECK(placed.object_tree->group()->shownPanels().size() == 1);
    CHECK(placed.object_tree->group()->shownPanels().contains(placed.object_tree));

    // 取消：归还源分组
    drag.cancel();
    CHECK(drag.phase() == dock::DragSession::Phase::Idle);
    CHECK(dock::DockCatalog::self().windows().isEmpty());
    CHECK(placed.console->group() == placed.object_tree->group());
    CHECK(placed.object_tree->group()->shownPanels().size() == 2);
    CHECK_FALSE(placed.console->isDetached());

    // 再次拖出到空白处：保留浮动，回停按钮可归还源分组
    dock::DragHandle tab_handle2(nullptr, placed.object_tree->group(), nullptr, placed.console);
    drag.beginAt(&tab_handle2, tab_press_point());
    drag.updateAt(tab_press_point() + QPoint(60, 0));
    REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);
    drag.endAt(QPoint(-1000, -1000));
    CHECK(drag.phase() == dock::DragSession::Phase::Idle);
    REQUIRE(dock::DockCatalog::self().windows().size() == 1);
    {
        dock::DockWindow* window = dock::DockCatalog::self().windows().first();
        REQUIRE(window->group() != nullptr);
        CHECK(window->group()->panels().contains(placed.console));
        REQUIRE(drag.reattachGroup(window->group()));
    }
    CHECK(placed.console->group() == placed.object_tree->group());
    CHECK(dock::DockCatalog::self().windows().isEmpty());
    CHECK_FALSE(placed.console->isDetached());
}
