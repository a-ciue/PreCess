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
#include "docking/DropResolver.h"
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

TEST_CASE("DockDrag: docking a parked single-tab float keeps the group alive")
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
        drag.beginAt(&handle, target.center() + QPoint(-50, -50));
        drag.updateAt(target.center());
        REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);
        drag.endAt(target.center());
    }
    dock::PanelGroup* merged_group = placed.object_tree->group();
    REQUIRE(placed.console->group() == merged_group);
    REQUIRE(merged_group->shownPanels().size() == 2);

    // 拖出 console 标签到空白处：浮动（临时分组成为浮窗的 QObject 子对象）
    const QPoint press = merged_group->node()->geometry().center();
    {
        dock::DragHandle tab_handle(nullptr, merged_group, nullptr, placed.console);
        drag.beginAt(&tab_handle, press);
        drag.updateAt(press + QPoint(60, 0));
        REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);
        drag.endAt(QPoint(-1000, -1000));
    }
    REQUIRE(dock::DockCatalog::self().windows().size() == 1);
    dock::DockWindow* parked_window = dock::DockCatalog::self().windows().first();
    dock::PanelGroup* parked_group = parked_window->group();
    REQUIRE(parked_group != nullptr);
    REQUIRE(parked_group->panels().contains(placed.console));

    // 再拖动该浮窗整组，停靠到 merged_group 的上边缘（分栏落点，回归“浮出后拖回停靠”崩溃）
    const QRect target_rect = merged_group->node()->geometry();
    QPoint inner_top_center;
    for (const dock::ZoneGeometry::ZoneRect& zone : dock::ZoneGeometry::innerZones(target_rect)) {
        if (zone.location == dock::DropZone::InnerTop)
            inner_top_center = zone.rect.center();
    }
    REQUIRE(inner_top_center != QPoint());

    {
        dock::DragHandle window_handle(nullptr, parked_group, parked_window);
        drag.beginAt(&window_handle, parked_window->geometry().center());
        drag.updateAt(inner_top_center);
        REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);
        CHECK(drag.hoveredZone() == dock::DropZone::InnerTop);
        drag.endAt(inner_top_center);
    }

    // 停靠后分组必须存活且节点 client 指回分组（不得随浮窗销毁）
    CHECK(dock::DockCatalog::self().windows().isEmpty());
    CHECK(placed.console->group() == parked_group);
    REQUIRE(parked_group->node() != nullptr);
    CHECK(parked_group->node()->client() == static_cast<dock::LayoutClient*>(parked_group));
    CHECK_FALSE(placed.console->isDetached());

#ifdef QT_DEBUG
    f.host.region()->validateTree();
#endif

    // 触发布局路径（原崩溃点）：显隐面板不再崩溃
    placed.console->hidePanel();
    CHECK_FALSE(placed.console->isShown());
    placed.console->showPanel();
    CHECK(placed.console->isShown());

    // 合并落点同样保持分组存活（整组从主区域拖出时目标几何会变化，需在越阈后再取）
    dock::DragHandle window_handle2(nullptr, parked_group, nullptr);
    {
        drag.beginAt(&window_handle2, parked_group->node()->geometry().center());
        drag.updateAt(parked_group->node()->geometry().center() + QPoint(0, -60));
        REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);

        const QRect merged_rect = merged_group->node()->geometry();
        drag.updateAt(merged_rect.center());
        CHECK(drag.hoveredZone() == dock::DropZone::Merge);
        drag.endAt(merged_rect.center());
    }
    CHECK(dock::DockCatalog::self().windows().isEmpty());
    CHECK(placed.console->group() == merged_group);
    CHECK(merged_group->node() != nullptr);
    CHECK(merged_group->node()->client() == static_cast<dock::LayoutClient*>(merged_group));

#ifdef QT_DEBUG
    f.host.region()->validateTree();
#endif
}

TEST_CASE("DockDrag: hiding the last panel closes its floating window")
{
    DockFixture f;
    PlacedDocks placed(f);

    dock::DragSession& drag = dock::DragSession::self();

    // 整组浮动：隐藏最后一个面板 → 自动回停并销毁浮窗
    placed.console->showPanel();
    REQUIRE(drag.detachGroup(placed.console->group()));
    REQUIRE(dock::DockCatalog::self().windows().size() == 1);
    CHECK(placed.console->group()->vacancy() != nullptr);

    placed.console->hidePanel();
    CHECK(dock::DockCatalog::self().windows().isEmpty());
    CHECK(placed.console->group()->vacancy() == nullptr);
    CHECK_FALSE(placed.console->isDetached());
    REQUIRE(placed.console->group()->node() != nullptr);
    CHECK_FALSE(placed.console->group()->node()->isVisible());

    // 重新显示：回到原停靠位置
    placed.console->showPanel();
    CHECK(placed.console->group()->node()->isVisible());
}

TEST_CASE("DockDrag: hiding a parked single-tab float returns it to origin")
{
    DockFixture f;
    PlacedDocks placed(f);

    dock::DragSession& drag = dock::DragSession::self();
    placed.console->showPanel();

    // 先把 console 合并到 objectTree 分组（中心落点）
    REQUIRE(drag.detachGroup(placed.console->group()));
    {
        dock::DockWindow* window = dock::DockCatalog::self().windows().first();
        dock::DragHandle handle(nullptr, placed.console->group(), window);
        const QRect target = placed.object_tree->group()->node()->geometry();
        drag.beginAt(&handle, target.center() + QPoint(-50, -50));
        drag.updateAt(target.center());
        REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);
        drag.endAt(target.center());
    }
    REQUIRE(placed.console->group() == placed.object_tree->group());
    REQUIRE(placed.object_tree->group()->shownPanels().size() == 2);

    // 拖出 console 标签到空白处：保留浮动（临时分组）
    const QPoint press = placed.object_tree->group()->node()->geometry().center();
    dock::DragHandle tab_handle(nullptr, placed.object_tree->group(), nullptr, placed.console);
    drag.beginAt(&tab_handle, press);
    drag.updateAt(press + QPoint(60, 0));
    REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);
    drag.endAt(QPoint(-1000, -1000));
    REQUIRE(dock::DockCatalog::self().windows().size() == 1);

    // 隐藏该面板 → 浮窗自动关闭并归还源分组
    placed.console->hidePanel();
    CHECK(dock::DockCatalog::self().windows().isEmpty());
    CHECK(placed.console->group() == placed.object_tree->group());
    CHECK_FALSE(placed.console->isDetached());
    CHECK_FALSE(placed.console->isShown());
    CHECK(placed.object_tree->group()->shownPanels().size() == 1);
}

TEST_CASE("DockDrag: multi-group floating window keeps other groups")
{
    DockFixture f;
    PlacedDocks placed(f);
    placed.object_tree->showPanel();
    placed.console->showPanel();

    dock::DragSession& drag = dock::DragSession::self();

    // 1) 浮出 console 分组（主分组）
    REQUIRE(drag.detachGroup(placed.console->group()));
    REQUIRE(dock::DockCatalog::self().windows().size() == 1);
    dock::DockWindow* window = dock::DockCatalog::self().windows().first();
    dock::PanelGroup* primary = window->group();
    REQUIRE(primary == placed.console->group());

    const auto window_zone_center = [window](dock::PanelGroup* group, dock::DropZone location) {
        const QRect group_rect(window->geometry().topLeft() + group->node()->geometry().topLeft(),
            group->node()->geometry().size());
        return indicatorRect(dock::ZoneGeometry::innerZones(group_rect), location).center();
    };

    // 2) 把 objectTree 分组整组拖进该浮窗内部做分栏（次级分组）
    {
        dock::DragHandle handle(nullptr, placed.object_tree->group(), nullptr);
        const QPoint press = placed.object_tree->group()->node()->geometry().center();
        drag.beginAt(&handle, press);
        drag.updateAt(press + QPoint(0, -60));
        REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);

        const QPoint inner_top = window_zone_center(primary, dock::DropZone::InnerTop);
        REQUIRE(inner_top != QPoint());
        drag.updateAt(inner_top);
        CHECK(drag.hoveredZone() == dock::DropZone::InnerTop);
        drag.endAt(inner_top);
    }

    // 浮窗承载两个分组：次级分组停靠不得导致窗口/其他分组消失
    REQUIRE(dock::DockCatalog::self().windows().size() == 1);
    CHECK(window->region()->groups().size() == 2);
    CHECK(placed.object_tree->group() != primary);
    CHECK(placed.object_tree->group()->parent() == window->region());

    // 3) 次级分组拖回主区域：浮窗与主分组保留
    {
        dock::DragHandle handle(nullptr, placed.object_tree->group(), window);
        const QPoint press = window->geometry().topLeft()
            + placed.object_tree->group()->node()->geometry().center();
        drag.beginAt(&handle, press);
        drag.updateAt(press + QPoint(0, -60));
        REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);

        const QRect host_rect(0, 0, 1600, 900);
        const QPoint outer_left = indicatorRect(
            dock::ZoneGeometry::outerZones(host_rect), dock::DropZone::OuterLeft).center();
        REQUIRE(outer_left != QPoint());
        drag.updateAt(outer_left);
        CHECK(drag.hoveredZone() == dock::DropZone::OuterLeft);
        drag.endAt(outer_left);
    }

    REQUIRE(dock::DockCatalog::self().windows().size() == 1);
    CHECK(window->region()->groups().size() == 1);
    CHECK(placed.object_tree->isShown());
    REQUIRE(placed.object_tree->group()->node() != nullptr);
    CHECK(placed.object_tree->group()->node()->client()
        == static_cast<dock::LayoutClient*>(placed.object_tree->group()));

    // 4) 再停靠回浮窗内部；隐藏两个分组全部面板 → 整窗自动回收回主区域
    {
        dock::DragHandle handle(nullptr, placed.object_tree->group(), nullptr);
        const QPoint press = placed.object_tree->group()->node()->geometry().center();
        drag.beginAt(&handle, press);
        drag.updateAt(press + QPoint(0, -60));
        REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);

        const QPoint inner_left = window_zone_center(primary, dock::DropZone::InnerLeft);
        REQUIRE(inner_left != QPoint());
        drag.updateAt(inner_left);
        drag.endAt(inner_left);
    }
    REQUIRE(dock::DockCatalog::self().windows().size() == 1);
    REQUIRE(window->region()->groups().size() == 2);

    placed.object_tree->hidePanel();
    placed.console->hidePanel();
    CHECK(dock::DockCatalog::self().windows().isEmpty());
    CHECK_FALSE(placed.console->isDetached());
    CHECK_FALSE(placed.object_tree->isDetached());
    REQUIRE(placed.console->group() != nullptr);
    REQUIRE(placed.object_tree->group() != nullptr);
    REQUIRE(placed.object_tree->group()->node() != nullptr);
    CHECK(placed.object_tree->group()->node()->client()
        == static_cast<dock::LayoutClient*>(placed.object_tree->group()));

    placed.console->showPanel();
    placed.object_tree->showPanel();
    CHECK(placed.console->isShown());
    CHECK(placed.object_tree->isShown());

#ifdef QT_DEBUG
    f.host.region()->validateTree();
#endif
}

TEST_CASE("DockDrag: drop resolver prefers floating windows and suppresses center merge on central group")
{
    DockFixture f;
    PlacedDocks placed(f);

    // 非中央分组中心：中心合并
    const QPoint object_tree_center = placed.object_tree->group()->node()->geometry().center();
    const dock::DropTarget merge_target = dock::DropResolver::resolve(object_tree_center, nullptr);
    CHECK(merge_target.region == f.host.region());
    CHECK(merge_target.group == placed.object_tree->group());
    CHECK(merge_target.zone == dock::DropZone::Merge);

    // 中央持久分组中心：无落点
    const QPoint central_center = f.host.centralGroup()->node()->geometry().center();
    const dock::DropTarget central_target = dock::DropResolver::resolve(central_center, nullptr);
    CHECK(central_target.group == f.host.centralGroup());
    CHECK(central_target.zone == dock::DropZone::None);

    // 浮窗优先：浮出 console 后其窗口中心命中浮窗区域
    placed.console->showPanel();
    dock::DragSession& drag = dock::DragSession::self();
    REQUIRE(drag.detachGroup(placed.console->group()));
    REQUIRE(dock::DockCatalog::self().windows().size() == 1);
    dock::DockWindow* window = dock::DockCatalog::self().windows().first();

    const QPoint floating_center = window->geometry().center();
    const dock::DropTarget floating_target = dock::DropResolver::resolve(floating_center, nullptr);
    CHECK(floating_target.region == window->region());
    CHECK(floating_target.group == placed.console->group());
    CHECK(floating_target.zone == dock::DropZone::Merge);

    // 排除拖拽源区域：同一坐标回退到主区域
    const dock::DropTarget excluded_target
        = dock::DropResolver::resolve(floating_center, window->region());
    CHECK(excluded_target.region == f.host.region());
    CHECK(excluded_target.region != window->region());

    REQUIRE(drag.reattachGroup(placed.console->group()));
}

TEST_CASE("DockDrag: central group is protected from dragging and center merge")
{
    DockFixture f;
    PlacedDocks placed(f);

    dock::DragSession& drag = dock::DragSession::self();
    dock::PanelGroup* central_group = f.host.centralGroup();
    REQUIRE(central_group != nullptr);

    // 命令式浮出与布局树摘出均被拒绝
    CHECK_FALSE(drag.detachGroup(central_group));
    CHECK(f.host.region()->extractGroupForWindow(central_group) == nullptr);

    // 直接构造的中央拖拽句柄不会进入拖拽状态
    dock::DragHandle central_handle(nullptr, central_group);
    drag.beginAt(&central_handle, QPoint(600, 400));
    drag.updateAt(QPoint(760, 400));
    CHECK(drag.phase() == dock::DragSession::Phase::Idle);
    CHECK(dock::DockCatalog::self().windows().isEmpty());

    // 浮出面板悬停中央分组中心：无中心落点，释放后保持浮动
    placed.console->showPanel();
    REQUIRE(drag.detachGroup(placed.console->group()));
    REQUIRE(dock::DockCatalog::self().windows().size() == 1);
    dock::DockWindow* window = dock::DockCatalog::self().windows().first();
    dock::DragHandle handle(nullptr, placed.console->group(), window);

    const QRect central_rect = central_group->node()->geometry();
    drag.beginAt(&handle, central_rect.center() + QPoint(0, -60));
    drag.updateAt(central_rect.center());
    REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);
    CHECK(drag.hoveredGroup() == central_group);
    CHECK(drag.hoveredZone() == dock::DropZone::None);

    drag.endAt(central_rect.center());
    CHECK(placed.console->isDetached());
    CHECK(dock::DockCatalog::self().windows().size() == 1);

    // 仅禁中心合并：悬停中央分组左侧内框仍可边缘分栏
    QPoint left_box;
    for (const dock::ZoneGeometry::ZoneRect& zone : dock::ZoneGeometry::innerZones(central_rect)) {
        if (zone.location == dock::DropZone::InnerLeft)
            left_box = zone.rect.center();
    }
    REQUIRE(left_box != QPoint());

    drag.beginAt(&handle, left_box + QPoint(0, -60));
    drag.updateAt(left_box);
    REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);
    CHECK(drag.hoveredGroup() == central_group);
    CHECK(drag.hoveredZone() == dock::DropZone::InnerLeft);
    drag.cancel();

    REQUIRE(drag.reattachGroup(placed.console->group()));
    CHECK(dock::DockCatalog::self().windows().isEmpty());
}

TEST_CASE("DockDrag: non-floatable targets never stay floating")
{
    DockFixture f;
    PlacedDocks placed(f);
    dock::DragSession& drag = dock::DragSession::self();
    placed.object_tree->showPanel();
    placed.console->showPanel();

    // 整组不可浮动：拖出后释放到无落点位置 → 回弹回原占位，不保留浮窗
    placed.console->setFeature(dock::DockPanel::Feature::Floatable, false);
    {
        dock::DragHandle handle(nullptr, placed.console->group(), nullptr);
        const QPoint press = placed.console->group()->node()->geometry().center();
        drag.beginAt(&handle, press);
        drag.updateAt(press + QPoint(0, -60));
        REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);
        drag.endAt(QPoint(-1000, -1000));
    }
    CHECK(drag.phase() == dock::DragSession::Phase::Idle);
    CHECK(dock::DockCatalog::self().windows().isEmpty());
    CHECK(placed.console->isShown());
    CHECK_FALSE(placed.console->isDetached());

    // 单标签不可浮动：释放到中央分组中心（无落点）→ 归还源分组
    placed.console->setFeature(dock::DockPanel::Feature::Floatable, true);
    dock::PanelGroup* group = placed.object_tree->group();
    REQUIRE(group != nullptr);
    dock::DockPanel* extra = f.makeDock(QStringLiteral("extra"), QStringLiteral("附加面板"));
    group->addPanel(extra);
    extra->showPanel();
    extra->setFeature(dock::DockPanel::Feature::Floatable, false);
    {
        const QRect group_rect = group->node()->geometry();
        dock::DragHandle handle(nullptr, group, nullptr, extra);
        drag.beginAt(&handle, group_rect.center());
        drag.updateAt(group_rect.center() + QPoint(60, 0));
        REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);
        drag.endAt(f.host.centralGroup()->node()->geometry().center());
    }
    CHECK(dock::DockCatalog::self().windows().isEmpty());
    CHECK(extra->group() == group);
    CHECK(extra->isShown());
    CHECK(placed.object_tree->isShown());
    CHECK_FALSE(extra->isDetached());
}

TEST_CASE("DockDrag: group docked inside a floating window reattaches to the host area")
{
    DockFixture f;
    PlacedDocks placed(f);
    placed.object_tree->showPanel();
    placed.console->showPanel();

    dock::DragSession& drag = dock::DragSession::self();
    REQUIRE(drag.detachGroup(placed.console->group()));
    REQUIRE(dock::DockCatalog::self().windows().size() == 1);
    dock::DockWindow* window = dock::DockCatalog::self().windows().first();
    dock::PanelGroup* primary = window->group();
    REQUIRE(primary == placed.console->group());

    const auto window_zone_center = [window](dock::PanelGroup* group, dock::DropZone location) {
        const QRect group_rect(window->geometry().topLeft() + group->node()->geometry().topLeft(),
            group->node()->geometry().size());
        return indicatorRect(dock::ZoneGeometry::innerZones(group_rect), location).center();
    };

    // 1) 把 objectTree 整组拖进浮窗内部做次级分组
    {
        dock::DragHandle handle(nullptr, placed.object_tree->group(), nullptr);
        const QPoint press = placed.object_tree->group()->node()->geometry().center();
        drag.beginAt(&handle, press);
        drag.updateAt(press + QPoint(0, -60));
        REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);

        const QPoint inner_top = window_zone_center(primary, dock::DropZone::InnerTop);
        REQUIRE(inner_top != QPoint());
        drag.updateAt(inner_top);
        REQUIRE(drag.hoveredZone() == dock::DropZone::InnerTop);
        drag.endAt(inner_top);
    }
    REQUIRE(window->region()->groups().size() == 2);
    dock::PanelGroup* secondary = placed.object_tree->group();
    REQUIRE(secondary != primary);

    // 2) 次级分组同样属于浮动状态：浮动命令不重复开窗，回停停回主区域
    CHECK(dock::DragSession::isFloating(secondary));
    CHECK_FALSE(drag.detachGroup(secondary));
    REQUIRE(drag.toggleDetached(secondary));
    CHECK(dock::DockCatalog::self().windows().size() == 1);
    CHECK(secondary->parent() == f.host.region());
    CHECK(f.host.region()->groups().contains(secondary));
    CHECK_FALSE(dock::DragSession::isFloating(secondary));

    // 3) 主分组回停后窗口回收
    REQUIRE(drag.reattachGroup(primary));
    CHECK(dock::DockCatalog::self().windows().isEmpty());

#ifdef QT_DEBUG
    f.host.region()->validateTree();
#endif
}

TEST_CASE("DockDrag: drag threshold uses manhattan distance")
{
    DockFixture f;
    PlacedDocks placed(f);
    placed.console->showPanel();

    dock::DragSession& drag = dock::DragSession::self();
    dock::DragHandle handle(nullptr, placed.console->group(), nullptr);
    const QPoint press = placed.console->group()->node()->geometry().center();
    drag.beginAt(&handle, press);
    REQUIRE(drag.phase() == dock::DragSession::Phase::Armed);

    // 单轴均未达阈值，但曼哈顿距离已达阈值：应进入拖拽（与会话内判定一致）
    drag.updateAt(press + QPoint(3, 3));
    CHECK(drag.phase() == dock::DragSession::Phase::Dragging);

    drag.cancel();
    CHECK(dock::DockCatalog::self().windows().isEmpty());
    CHECK(placed.console->isShown());
    CHECK_FALSE(placed.console->isDetached());
}

TEST_CASE("DockDrag: fixture cleanup evacuates floating windows on scope exit")
{
    {
        DockFixture f;
        PlacedDocks placed(f);
        placed.console->showPanel();

        REQUIRE(dock::DragSession::self().detachGroup(placed.console->group()));
        REQUIRE(dock::DockCatalog::self().windows().size() == 1);
    } // DockFixture 析构：RAII 兜底清理

    CHECK(dock::DockCatalog::self().windows().isEmpty());
    CHECK(dock::DragSession::self().phase() == dock::DragSession::Phase::Idle);
    CHECK(dock::DockCatalog::self().host() == nullptr);
}