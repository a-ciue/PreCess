/**
 * @file TestDockCore.cpp
 * @brief 停靠核心语义单元测试：命令式停靠布局、关闭/恢复、中央部件、选项卡
 */

#include "DockTestSupport.h"

#include "docking/DockCatalog.h"
#include "docking/DockPanel.h"
#include "docking/DockRegion.h"
#include "docking/DragSession.h"
#include "docking/PanelGroup.h"
#include "docking/DockHost.h"
#include "tree/LayoutNode.h"
#include "tree/BoxNode.h"

#include <catch2/catch_test_macros.hpp>

using namespace docktest;

TEST_CASE("DockCore: initial layout matches Main.qml placement")
{
    DockFixture f;
    PlacedDocks placed(f);

    dock::DockPanel* central = f.host.centralPanel();
    REQUIRE(central != nullptr);
    REQUIRE(central->isShown());

    // 对象树固定在左侧 250px，纵向与属性列表分占
    const dock::LayoutNode* object_tree_item = placed.object_tree->group()->node();
    REQUIRE(object_tree_item != nullptr);
    CHECK(object_tree_item->isVisible());
    CHECK(object_tree_item->geometry().x() == 0);
    CHECK(object_tree_item->geometry().width() == 250);
    CHECK(object_tree_item->geometry().height() > 0);
    CHECK(object_tree_item->geometry().height() < 900);

    // 中央部件占据其余宽度，且是受保护节点
    const dock::LayoutNode* central_item = central->group()->node();
    REQUIRE(central_item != nullptr);
    CHECK(f.host.region()->isCentralNode(central_item));
    CHECK(central_item->geometry().x() == 250 + dock::kDividerThickness);
    CHECK(central_item->geometry().width() == 1600 - 250 - dock::kDividerThickness);
    CHECK(central_item->geometry().height() == 900);

    // 属性列表停靠在对象树下方，高度约 400
    const dock::LayoutNode* side_bar_item = placed.side_bar->group()->node();
    REQUIRE(side_bar_item != nullptr);
    CHECK(side_bar_item->isVisible());
    CHECK(side_bar_item->geometry().width() == 250);
    const int side_bar_height = side_bar_item->geometry().height();
    CHECK(side_bar_height >= 398);
    CHECK(side_bar_height <= 403);
    CHECK(object_tree_item->geometry().height() + dock::kDividerThickness + side_bar_height == 900);
}

TEST_CASE("DockCore: PanelLaunch::Hidden docks stay closed until opened")
{
    DockFixture f;
    PlacedDocks placed(f);

    const dock::DockPanel* hidden_docks[] = { placed.attribute_render, placed.console,
        placed.python_console, placed.output_log, placed.preferences };
    for (const dock::DockPanel* panel : hidden_docks) {
        CHECK_FALSE(panel->isShown());
        REQUIRE(panel->group() != nullptr);
        CHECK_FALSE(panel->group()->node()->isVisible());
    }
}

TEST_CASE("DockCore: open shows hidden dock at remembered position")
{
    DockFixture f;
    PlacedDocks placed(f);

    const dock::LayoutNode* object_tree_item = placed.object_tree->group()->node();
    const dock::LayoutNode* side_bar_item = placed.side_bar->group()->node();
    const int side_bar_height = side_bar_item->geometry().height();

    // 打开底部日志：占据布局底部约 300px
    placed.console->showPanel();
    CHECK(placed.console->isShown());
    const dock::LayoutNode* console_item = placed.console->group()->node();
    REQUIRE(console_item != nullptr);
    CHECK(console_item->isVisible());
    CHECK(console_item->geometry().width() == 1600);
    const int console_height = console_item->geometry().height();
    CHECK(console_height >= 298);
    CHECK(console_height <= 303);

    // 左侧区域高度收缩
    const int left_total = object_tree_item->geometry().height() + dock::kDividerThickness
        + side_bar_item->geometry().height();
    const int expected_left_total = 900 - dock::kDividerThickness - console_height;
    const int left_difference = left_total - expected_left_total;
    CHECK(left_difference >= -2);
    CHECK(left_difference <= 2);

    // 再次关闭后恢复原状（占比换算取整允许 1~2px 漂移）
    placed.console->hidePanel();
    CHECK_FALSE(placed.console->isShown());
    CHECK_FALSE(console_item->isVisible());
    const int restored_total = object_tree_item->geometry().height()
        + dock::kDividerThickness + side_bar_height;
    CHECK(restored_total >= 898);
    CHECK(restored_total <= 900);
}

TEST_CASE("DockCore: closing and reopening a docked panel restores its size")
{
    DockFixture f;
    PlacedDocks placed(f);

    const dock::LayoutNode* object_tree_item = placed.object_tree->group()->node();
    const dock::LayoutNode* side_bar_item = placed.side_bar->group()->node();
    const int side_bar_height_before = side_bar_item->geometry().height();

    placed.object_tree->hidePanel();
    CHECK_FALSE(placed.object_tree->isShown());
    CHECK_FALSE(object_tree_item->isVisible());
    CHECK(side_bar_item->geometry().height() == 900);

    placed.object_tree->showPanel();
    CHECK(placed.object_tree->isShown());
    CHECK(object_tree_item->isVisible());
    const int difference = side_bar_item->geometry().height() - side_bar_height_before;
    CHECK(difference >= -1);
    CHECK(difference <= 1);
}

TEST_CASE("DockCore: insert and move panels keep shown order")
{
    DockFixture f;
    PlacedDocks placed(f);

    dock::PanelGroup* group = placed.object_tree->group();
    REQUIRE(group != nullptr);

    // 组内两个显示面板 + 一个隐藏面板（隐藏面板不参与显示下标）
    dock::DockPanel* extra = f.makeDock(QStringLiteral("extra"), QStringLiteral("附加面板"));
    group->addPanel(extra);
    extra->showPanel();
    dock::DockPanel* hidden = f.makeDock(QStringLiteral("hidden"), QStringLiteral("隐藏面板"));
    group->addPanel(hidden);

    QList<dock::DockPanel*> shown = group->shownPanels();
    REQUIRE(shown.size() == 2);
    CHECK(shown.at(0) == placed.object_tree);
    CHECK(shown.at(1) == extra);

    // 插到第一个显示面板之前
    dock::DockPanel* first = f.makeDock(QStringLiteral("first"), QStringLiteral("最前"));
    group->insertPanel(first, 0);
    first->showPanel();
    shown = group->shownPanels();
    REQUIRE(shown.size() == 3);
    CHECK(shown.at(0) == first);
    CHECK(shown.at(1) == placed.object_tree);
    CHECK(shown.at(2) == extra);

    // 追加到最后（隐藏面板不影响显示序列）
    dock::DockPanel* last = f.makeDock(QStringLiteral("last"), QStringLiteral("最后"));
    group->insertPanel(last, group->shownPanels().size());
    last->showPanel();
    shown = group->shownPanels();
    REQUIRE(shown.size() == 4);
    CHECK(shown.at(3) == last);
    CHECK(group->panels().contains(hidden));
    CHECK_FALSE(hidden->isShown());

    // 重排：把第一个移到第二个标签之后，激活面板不变
    CHECK(group->movePanel(0, 2));
    shown = group->shownPanels();
    REQUIRE(shown.size() == 4);
    CHECK(shown.at(0) == placed.object_tree);
    CHECK(shown.at(1) == first);
    CHECK(shown.at(2) == extra);
    CHECK(shown.at(3) == last);
    CHECK(group->activePanel() == placed.object_tree);

    // 越界与无效操作
    CHECK_FALSE(group->movePanel(0, 0));
    CHECK_FALSE(group->movePanel(-1, 1));
    CHECK_FALSE(group->movePanel(0, 99));
}

TEST_CASE("DockCore: panel features gate detach and hide others")
{
    DockFixture f;
    PlacedDocks placed(f);

    dock::PanelGroup* group = placed.object_tree->group();
    REQUIRE(group != nullptr);

    dock::DockPanel* extra = f.makeDock(QStringLiteral("extra"), QStringLiteral("附加面板"));
    group->addPanel(extra);
    extra->showPanel();

    // 默认能力位全开
    CHECK(group->features().testFlag(dock::DockPanel::Feature::Floatable));
    CHECK(group->features().testFlag(dock::DockPanel::Feature::Closable));
    CHECK(group->features().testFlag(dock::DockPanel::Feature::Movable));

    // 组内任一面板关闭浮动能力 → 整组不可拆出
    extra->setFeature(dock::DockPanel::Feature::Floatable, false);
    CHECK_FALSE(group->features().testFlag(dock::DockPanel::Feature::Floatable));
    CHECK_FALSE(dock::DragSession::self().detachGroup(group));

    // 恢复能力后可正常拆出与回停
    extra->setFeature(dock::DockPanel::Feature::Floatable, true);
    REQUIRE(dock::DragSession::self().detachGroup(group));
    REQUIRE(dock::DragSession::self().reattachGroup(group));

    // 关闭其他标签：只保留指定面板
    group->hideOthers(placed.object_tree);
    CHECK(placed.object_tree->isShown());
    CHECK_FALSE(extra->isShown());

    // 关闭其他分组：指定分组保留，其余隐藏，中央持久部件不受影响
    extra->showPanel();
    placed.console->showPanel();
    f.host.hideOtherGroups(group);
    CHECK(placed.object_tree->isShown());
    CHECK(extra->isShown());
    CHECK_FALSE(placed.console->isShown());
    CHECK(f.host.centralPanel()->isShown());

    // 中央持久分组无任何能力
    CHECK_FALSE(f.host.centralGroup()->features().testFlag(dock::DockPanel::Feature::Floatable));
    CHECK_FALSE(f.host.centralGroup()->features().testFlag(dock::DockPanel::Feature::Closable));
    CHECK_FALSE(f.host.centralGroup()->features().testFlag(dock::DockPanel::Feature::Movable));
}

TEST_CASE("DockCore: dock widgets can tab into the central group")
{
    DockFixture f;
    PlacedDocks placed(f);

    dock::PanelGroup* central_group = f.host.centralGroup();
    REQUIRE(central_group != nullptr);
    const int tabs_before = central_group->panels().size();

    dock::DockPanel* extra = f.makeDock(QStringLiteral("extra"), QStringLiteral("附加面板"));
    f.host.stackPanel(extra);
    extra->applyShown(true);

    CHECK(central_group->panels().size() == tabs_before + 1);
    CHECK(central_group->shownPanels().contains(extra));

    central_group->setActivePanel(extra);
    CHECK(central_group->activePanel() == extra);
    CHECK(central_group->activeIndex() == central_group->shownPanels().indexOf(extra));
    CHECK(central_group->title() == QStringLiteral("附加面板"));
}

TEST_CASE("DockCore: hovering decides the Ctrl+Tab tab target")
{
    DockFixture f;
    PlacedDocks placed(f);

    // 悬停分组多标签：循环切换其标签
    dock::DockPanel* extra = f.makeDock(QStringLiteral("extra"), QStringLiteral("附加面板"));
    placed.object_tree->group()->addPanel(extra);
    extra->showPanel();
    REQUIRE(placed.object_tree->group()->shownPanels().size() == 2);

    const QPoint object_tree_center = placed.object_tree->group()->node()->geometry().center();
    REQUIRE(dock::DockCatalog::self().cyclePanelAt(object_tree_center, true));
    CHECK(placed.object_tree->group()->activePanel() == extra);
    REQUIRE(dock::DockCatalog::self().cyclePanelAt(object_tree_center, false));
    CHECK(placed.object_tree->group()->activePanel() == placed.object_tree);

    // 悬停单标签分组：不切换
    const QPoint side_bar_center = placed.side_bar->group()->node()->geometry().center();
    CHECK_FALSE(dock::DockCatalog::self().cyclePanelAt(side_bar_center, true));

    // 未悬停到任何分组：不切换
    CHECK_FALSE(dock::DockCatalog::self().cyclePanelAt(QPoint(-10, -10), true));
}

TEST_CASE("DockCore: dock registry tracks dock widgets and main window")
{
    DockFixture f;
    PlacedDocks placed(f);

    CHECK(dock::DockCatalog::self().host() == &f.host);
    CHECK(dock::DockCatalog::self().panels().contains(placed.object_tree));
    CHECK(dock::DockCatalog::self().panels().size() == 8); // 7 + 中央持久面板
}
