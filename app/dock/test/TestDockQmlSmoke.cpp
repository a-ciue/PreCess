/**
 * @file TestDockQmlSmoke.cpp
 * @brief QML 集成冒烟测试：类型注册、中央持久部件、重开面板、拖拽浮窗回归
 *
 * 使用 offscreen 平台运行，覆盖：
 * - PreCess.Docking 类型注册与 Enums 枚举访问；
 * - centralItemFile 加载与 client 挂载（中央渲染窗口问题回归）；
 * - 面板关闭后重开 client 恢复可见（“重开空白”问题回归）；
 * - 真实浮窗视图下的浮动 → 拖拽合并（“拖动崩溃”问题回归）。
 */

#include "Docking.h"
#include "DockTestSupport.h"

#include "docking/DockCatalog.h"
#include "docking/DockPanel.h"
#include "docking/DragSession.h"
#include "docking/DragHandle.h"
#include "docking/DockRegion.h"
#include "docking/DockWindow.h"
#include "docking/PanelGroup.h"
#include "docking/DockHost.h"
#include "docking/DockView.h"
#include "tree/LayoutNode.h"
#include "ui/PanelGroupItem.h"
#include "ui/DockHostItem.h"
#include "ui/DropZoneOverlay.h"
#include "ui/DockRuntime.h"

#include <catch2/catch_test_macros.hpp>

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QTest>
#include <QtQuickTest/quicktest.h>

namespace {

//! @brief 进程内单例 QGuiApplication（offscreen）
//!
//! QGuiApplication 为进程级单例，若在多个 TEST_CASE 内各自构造会因重复实例崩溃；
//! 统一经本函数惰性构造，保证与用例数量无关。
QGuiApplication& testApplication()
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    static int argc = 1;
    static char app_name[] = "TestDockQmlSmoke";
    static char* argv[] = { app_name, nullptr };
    static QGuiApplication app(argc, argv);
    return app;
}

} // namespace

TEST_CASE("DockQml: docking area loads from QML and lays out docks")
{
    testApplication();

    // 断言失败提前退出时的兜底清理（共享单例：DockCatalog/DragSession）
    docktest::DockSessionCleanup cleanup;

    QQmlEngine engine;
    dock::init(&engine);

    QQmlComponent component(&engine);
    component.setData(R"QML(
        import QtQuick
        import PreCess.Docking as Docking

        Docking.DockHost {
            id: area
            width: 800
            height: 600
            uniqueName: "smoke"
            centralItemFile: "qrc:/precess/dock/test/CentralStub.qml"

            Docking.DockPanel {
                id: panelA
                uniqueName: "panelA"
                title: "Panel A"
                Item { anchors.fill: parent }
            }

            Docking.DockPanel {
                id: panelB
                uniqueName: "panelB"
                title: "Panel B"
                Item { anchors.fill: parent }
            }

            Docking.DockPanel {
                id: panelC
                uniqueName: "panelC"
                title: "Panel C"
                Item { anchors.fill: parent }
            }

            Component.onCompleted: {
                area.placePanel(panelA, Docking.Tokens.DockEdge.Left, null, Qt.size(200, 0))
                area.placePanel(panelB, Docking.Tokens.DockEdge.Bottom, null, Qt.size(0, 150),
                                   Docking.Tokens.PanelLaunch.Hidden)
                area.placePanel(panelC, Docking.Tokens.DockEdge.Top, null, Qt.size(0, 150),
                                   Docking.Tokens.PanelLaunch.Hidden)
            }
        }
    )QML",
        QUrl());

    if (component.isError())
        FAIL(component.errorString().toStdString());
    REQUIRE_FALSE(component.isError());

    QObject* root = component.create();
    REQUIRE(root != nullptr);

    auto* area_item = qobject_cast<QQuickItem*>(root);
    REQUIRE(area_item != nullptr);
    area_item->setWidth(800);
    area_item->setHeight(600);

    // 挂到真实窗口：QML 布局与 TabBar 的 ListView 需要窗口 polish 才会定位标签
    QQuickWindow host_window;
    host_window.resize(800, 600);
    area_item->setParentItem(host_window.contentItem());
    host_window.show();
    QCoreApplication::processEvents();

    auto* platform = &dock::ui::DockRuntime::instance();

    // 基础：注册表与开关状态
    dock::DockHost* host = dock::DockCatalog::self().host();
    REQUIRE(host != nullptr);
    CHECK(dock::DockCatalog::self().panels().size() == 4); // panelA + panelB + panelC + 中央

    dock::DockPanel* panel_a = nullptr;
    dock::DockPanel* panel_b = nullptr;
    dock::DockPanel* panel_c = nullptr;
    for (dock::DockPanel* panel : dock::DockCatalog::self().panels()) {
        if (panel->uniqueName() == QStringLiteral("panelA"))
            panel_a = panel;
        else if (panel->uniqueName() == QStringLiteral("panelB"))
            panel_b = panel;
        else if (panel->uniqueName() == QStringLiteral("panelC"))
            panel_c = panel;
    }
    REQUIRE(panel_a != nullptr);
    REQUIRE(panel_b != nullptr);
    REQUIRE(panel_c != nullptr);
    CHECK(panel_a->isShown());
    CHECK_FALSE(panel_b->isShown());
    CHECK_FALSE(panel_c->isShown());

    // 布局：左侧面板 200 宽；隐藏面板不占空间
    dock::LayoutNode* panel_a_item = panel_a->group()->node();
    REQUIRE(panel_a_item != nullptr);
    CHECK(panel_a_item->isVisible());
    CHECK(panel_a_item->geometry().width() == 200);
    CHECK(panel_a_item->geometry().height() == 600);
    CHECK_FALSE(panel_b->group()->node()->isVisible());

    // 中央持久部件：client 已挂载且可见
    dock::DockPanel* central = host->centralPanel();
    REQUIRE(central != nullptr);
    QQuickItem* central_guest = platform->panelContentItem(central);
    REQUIRE(central_guest != nullptr);
    CHECK(central_guest->isVisible());
    CHECK(central_guest->width() > 0);
    CHECK(central_guest->height() > 0);
    REQUIRE(central_guest->parentItem() != nullptr);
    CHECK(central_guest->parentItem()->objectName() == QStringLiteral("contentArea"));

    // 动态标题必须同步到真实停靠标题栏，恢复默认标题后继续既有布局用例。
    auto* initial_view = platform->panelGroupItem(panel_a->group());
    REQUIRE(initial_view != nullptr);
    auto* initial_title = initial_view->findChild<QQuickItem*>(QStringLiteral("titleText"));
    REQUIRE(initial_title != nullptr);
    panel_a->setTitle(QStringLiteral("操作面板-三角形网格生成"));
    QCoreApplication::processEvents();
    CHECK(initial_title->property("text").toString() == QStringLiteral("操作面板-三角形网格生成"));
    if (const auto capture = qEnvironmentVariable("PRECESS_DOCK_CAPTURE"); !capture.isEmpty()) {
        QTest::qWait(100);
        REQUIRE(host_window.grabWindow().save(capture));
    }
    panel_a->setTitle(QStringLiteral("Panel A"));
    QCoreApplication::processEvents();
    CHECK(initial_title->property("text").toString() == QStringLiteral("Panel A"));

    // 重开面板：client 应恢复可见（重开空白回归）
    QQuickItem* panel_a_guest = platform->panelContentItem(panel_a);
    REQUIRE(panel_a_guest != nullptr);
    panel_a->hidePanel();
    QCoreApplication::processEvents();
    CHECK_FALSE(panel_a_guest->isVisible());
    panel_a->showPanel();
    QCoreApplication::processEvents();
    CHECK(panel_a_guest->isVisible());

    // 打开底部面板，随后拖动合并（真实浮窗视图下的崩溃回归）
    panel_b->showPanel();
    QCoreApplication::processEvents();
    CHECK(panel_b->group()->node()->isVisible());

    // 关闭其他分组：panelA 分组保留，panelB 隐藏，中央持久部件不受影响
    {
        dock::ui::PanelGroupItem* view_a = platform->panelGroupItem(panel_a->group());
        REQUIRE(view_a != nullptr);
        view_a->hideOtherGroups();
        QCoreApplication::processEvents();
        CHECK(panel_a->isShown());
        CHECK_FALSE(panel_b->isShown());
        CHECK(host->centralPanel()->isShown());
        panel_b->showPanel();
        QCoreApplication::processEvents();
        CHECK(panel_b->group()->node()->isVisible());
    }

    dock::DragSession& drag = dock::DragSession::self();
    REQUIRE(drag.detachGroup(panel_a->group()));
    QCoreApplication::processEvents();

    REQUIRE(dock::DockCatalog::self().windows().size() == 1);
    dock::DockWindow* window = dock::DockCatalog::self().windows().first();
    CHECK(panel_a->isDetached());
    CHECK(window->group() == panel_a->group());
    // 浮窗视图在首个有效几何到达后自行显示（无首帧空白）
    REQUIRE(window->view() != nullptr);
    CHECK(window->view()->isShown());

    dock::DragHandle handle(nullptr, panel_a->group(), window);
    const QRect target_rect = panel_b->group()->node()->geometry();
    const QPoint target_center = target_rect.center();

    dock::ui::DropZoneOverlay* overlay = platform->zonesOverlay();
    REQUIRE(overlay != nullptr);
    CHECK_FALSE(overlay->isActive());

    // 先悬停到面板间的分隔条（不属于任何分组，但位于停靠区域内）：
    // 选择器仍出现，此时为 4 个外方框
    const QPoint gap_point(target_center.x(),
        target_rect.top() - dock::kDividerThickness / 2);
    drag.beginAt(&handle, target_center + QPoint(-50, -50));
    drag.updateAt(gap_point);
    REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);
    CHECK(drag.hoveredRegion() != nullptr);
    CHECK(drag.hoveredGroup() == nullptr);
    CHECK(overlay->isActive());
    CHECK(overlay->zoneCount() == 4);

    // 悬停中央持久分组：中心合并方框被抑制（内 4 - 1 + 外 4 = 8）
    const QRect central_rect = host->centralGroup()->node()->geometry();
    drag.updateAt(central_rect.center());
    REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);
    CHECK(drag.hoveredGroup() == host->centralGroup());
    CHECK(drag.hoveredZone() == dock::DropZone::None);
    CHECK(overlay->zoneCount() == 8);

    // 移到面板上：五个内方框 + 常显的四个外方框
    drag.updateAt(target_center);
    REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);
    CHECK(drag.hoveredZone() == dock::DropZone::Merge);
    // 落点高亮由顶层浮层窗口呈现（不被拖动中的浮窗遮挡）
    CHECK(overlay->isActive());
    CHECK(overlay->isVisible());
    CHECK(overlay->zoneCount() == 9);

    drag.endAt(target_center);
    QCoreApplication::processEvents();

    CHECK(drag.phase() == dock::DragSession::Phase::Idle);
    CHECK(panel_a->group() == panel_b->group());
    CHECK_FALSE(panel_a->isDetached());
    CHECK(dock::DockCatalog::self().windows().isEmpty());
    CHECK_FALSE(overlay->isActive());
    CHECK(overlay->zoneCount() == 0);

    // 合并后成为选项卡：切换为当前页后 client 可见，且无悬空视图
    CHECK(panel_b->group()->panels().contains(panel_a));
    QQuickItem* merged_guest = platform->panelContentItem(panel_a);
    REQUIRE(merged_guest != nullptr);
    panel_b->group()->setActivePanel(panel_a);
    QCoreApplication::processEvents();
    CHECK(merged_guest->isVisible());

    // 标签栏插入位置查询（真实 TabBar 几何）
    dock::ui::PanelGroupItem* merged_view = platform->panelGroupItem(panel_b->group());
    REQUIRE(merged_view != nullptr);
    QQuickItem* tab_bar = merged_view->findChild<QQuickItem*>(QStringLiteral("tabBar"));
    REQUIRE(tab_bar != nullptr);

    const auto tab_item = [tab_bar](int index) -> QQuickItem* {
        QQuickItem* item = nullptr;
        QMetaObject::invokeMethod(tab_bar, "itemAt", Q_RETURN_ARG(QQuickItem*, item),
            Q_ARG(int, index));
        return item;
    };
    const auto tab_center = [&tab_item](int index) -> QPointF {
        QQuickItem* item = tab_item(index);
        return item ? item->mapToGlobal(QPointF(item->width() / 2, item->height() / 2))
                    : QPointF();
    };

    // 标签模型变更后的 ListView 布局在 polish 阶段完成；处理事件并不保证坐标已更新。
    // 后续各场景在读取坐标前就地断言等待结果，保留具体失败位置，便于定位未完成布局的操作。
    REQUIRE(QQuickTest::qWaitForPolish(&host_window));
    QQuickItem* tab1 = tab_item(1);
    REQUIRE(tab1 != nullptr);
    const QPointF tab1_left = tab1->mapToGlobal(QPointF(tab1->width() * 0.25, tab1->height() / 2));
    const QPointF tab1_right = tab1->mapToGlobal(QPointF(tab1->width() * 0.75, tab1->height() / 2));
    CHECK(merged_view->tabInsertIndexAt(tab1_left.toPoint()) == 1);
    CHECK(merged_view->tabInsertIndexAt(tab1_right.toPoint()) == 2);
    CHECK(merged_view->tabInsertIndexAt(central_rect.center()) == -1);

    // 标签/标题模式：多标签显示标签行（每个标签常显关闭按钮）；单标签回到标题模式
    {
        REQUIRE(panel_b->group()->shownPanels().size() > 1);
        CHECK(tab_bar->isVisible());
        for (int i = 0; i < panel_b->group()->shownPanels().size(); ++i) {
            QQuickItem* tab = tab_item(i);
            REQUIRE(tab != nullptr);
            QQuickItem* close = tab->findChild<QQuickItem*>(QStringLiteral("tabClose"));
            REQUIRE(close != nullptr);
            CHECK(close->isVisible());
        }

        const QList<dock::DockPanel*> shown_before = panel_b->group()->shownPanels();
        for (int i = 1; i < shown_before.size(); ++i)
            shown_before.at(i)->hidePanel();
        QCoreApplication::processEvents();
        REQUIRE(panel_b->group()->shownPanels().size() == 1);
        CHECK_FALSE(tab_bar->isVisible());
        QQuickItem* title_text = merged_view->findChild<QQuickItem*>(QStringLiteral("titleText"));
        REQUIRE(title_text != nullptr);
        CHECK(title_text->isVisible());

        for (int i = 1; i < shown_before.size(); ++i)
            shown_before.at(i)->showPanel();
        QCoreApplication::processEvents();
        CHECK(tab_bar->isVisible());
        CHECK(panel_b->group()->shownPanels().size() == shown_before.size());
    }

    // 悬停标签栏拖放：命中的是合并落点 + 插入下标 + 插入标记线
    panel_c->showPanel();
    QCoreApplication::processEvents();
    REQUIRE(drag.detachGroup(panel_c->group()));
    QCoreApplication::processEvents();
    REQUIRE(dock::DockCatalog::self().windows().size() == 1);

    REQUIRE(QQuickTest::qWaitForPolish(&host_window));
    QQuickItem* tab1_after = tab_item(1);
    REQUIRE(tab1_after != nullptr);
    const QPointF insert_point
        = tab1_after->mapToGlobal(QPointF(tab1_after->width() * 0.25, tab1_after->height() / 2));

    dock::DockWindow* c_window = dock::DockCatalog::self().windows().first();
    dock::DragHandle c_handle(nullptr, panel_c->group(), c_window);
    drag.beginAt(&c_handle, insert_point.toPoint() + QPoint(-60, -60));
    drag.updateAt(insert_point.toPoint());
    REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);
    CHECK(drag.hoveredGroup() == panel_b->group());
    CHECK(drag.hoveredZone() == dock::DropZone::Merge);
    CHECK(drag.hoveredTabIndex() == 1);
    CHECK(overlay->isActive());
    CHECK_FALSE(overlay->tabInsertRect().isEmpty());

    drag.endAt(insert_point.toPoint());
    QCoreApplication::processEvents();
    CHECK(dock::DockCatalog::self().windows().isEmpty());
    CHECK_FALSE(overlay->isActive());
    {
        const QList<dock::DockPanel*> shown = panel_b->group()->shownPanels();
        REQUIRE(shown.size() == 3);
        CHECK(shown.at(0) == panel_b);
        CHECK(shown.at(1) == panel_c);
        CHECK(shown.at(2) == panel_a);
    }

    // 组内标签重排：横向拖拽显示标记线，释放时提交顺序（不进入拖拽会话）
    REQUIRE(QQuickTest::qWaitForPolish(&host_window));
    const QPointF tab0_center = tab_center(0);
    const QPointF second_tab_right = [&tab_item] {
        QQuickItem* item = tab_item(1);
        return item ? item->mapToGlobal(QPointF(item->width() * 0.75, item->height() / 2))
                    : QPointF();
    }();
    merged_view->beginPanelDrag(0, tab0_center);
    merged_view->dragTo(second_tab_right);
    CHECK(merged_view->isReordering());
    CHECK(merged_view->reorderMarkerX() >= 0);
    merged_view->endDrag(second_tab_right);
    CHECK_FALSE(merged_view->isReordering());
    CHECK(drag.phase() == dock::DragSession::Phase::Idle);
    {
        const QList<dock::DockPanel*> shown = panel_b->group()->shownPanels();
        REQUIRE(shown.size() == 3);
        CHECK(shown.at(0) == panel_c);
        CHECK(shown.at(1) == panel_b);
        CHECK(shown.at(2) == panel_a);
        CHECK(panel_b->group()->activePanel() == panel_b); // 激活面板不因重排改变
    }

    // 纵向拖拽标签仍走浮动流程：分离单个标签为浮窗并回停
    REQUIRE(QQuickTest::qWaitForPolish(&host_window));
    dock::DockPanel* dragged_panel = panel_b->group()->shownPanels().first();
    merged_view->beginPanelDrag(0, tab_center(0));
    merged_view->dragTo(tab_center(0) + QPointF(0, -80));
    REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);
    CHECK(dock::DockCatalog::self().windows().size() == 1);
    drag.endAt(QPoint(-1000, -1000)); // 空白处释放：保留浮动
    CHECK(drag.phase() == dock::DragSession::Phase::Idle);
    REQUIRE(dock::DockCatalog::self().windows().size() == 1);
    {
        dock::DockWindow* separated = dock::DockCatalog::self().windows().first();
        REQUIRE(separated->group() != nullptr);
        CHECK(separated->group()->panels().contains(dragged_panel));
        CHECK_FALSE(separated->group()->panels().contains(panel_a));
        REQUIRE(drag.reattachGroup(separated->group()));
    }
    CHECK(dock::DockCatalog::self().windows().isEmpty());
    CHECK(panel_b->group()->panels().contains(panel_a));

    // 关闭其他标签与能力门控（组内存在不可移动面板 → 整组不进入拖拽）
    {
        const QList<dock::DockPanel*> shown = panel_b->group()->shownPanels();
        REQUIRE(shown.size() >= 2);

        merged_view->hideOthers(0);
        QCoreApplication::processEvents();
        REQUIRE(panel_b->group()->shownPanels().size() == 1);
        CHECK(panel_b->group()->shownPanels().first() == shown.first());

        // 恢复多标签场景
        for (dock::DockPanel* panel : shown) {
            if (panel != shown.first())
                panel->showPanel();
        }
        QCoreApplication::processEvents();
        CHECK(panel_b->group()->shownPanels().size() >= 2);

        REQUIRE(QQuickTest::qWaitForPolish(&host_window));
        // 能力门控：单标签拖拽按面板能力，整组拖拽按组能力（显示面板交集）
        dock::DockPanel* gated = panel_b->group()->shownPanels().first();
        gated->setFeature(dock::DockPanel::Feature::Movable, false);
        QCoreApplication::processEvents();

        // 不可移动面板的标签：不进入拖拽
        merged_view->beginPanelDrag(0, tab_center(0));
        merged_view->dragTo(tab_center(0) + QPointF(0, -80));
        CHECK(drag.phase() == dock::DragSession::Phase::Idle);
        CHECK(dock::DockCatalog::self().windows().isEmpty());

        // 组能力为交集：标题行整组拖拽同样被门控
        merged_view->beginGroupDrag(tab_center(0));
        merged_view->dragTo(tab_center(0) + QPointF(0, -80));
        CHECK(drag.phase() == dock::DragSession::Phase::Idle);
        CHECK(dock::DockCatalog::self().windows().isEmpty());

        // 同级其他面板仍可拖动（按面板能力），取消后状态复原
        merged_view->beginPanelDrag(1, tab_center(1));
        merged_view->dragTo(tab_center(1) + QPointF(0, -80));
        REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);
        drag.cancel();
        QCoreApplication::processEvents();
        CHECK(dock::DockCatalog::self().windows().isEmpty());

        gated->setFeature(dock::DockPanel::Feature::Movable, true);
        QCoreApplication::processEvents();
        CHECK(panel_b->group()->shownPanels().size() == shown.size());
    }

    // 真实鼠标事件路径（回归：QML 移动回调失效导致合并后标签无法拖出/重排）
    {
        // A. 纵向拖出未激活标签 → 浮窗
        const QList<dock::DockPanel*> shown = panel_b->group()->shownPanels();
        REQUIRE(shown.size() >= 2);
        panel_b->group()->setActivePanel(shown.first());
        QCoreApplication::processEvents();

        REQUIRE(QQuickTest::qWaitForPolish(&host_window));
        QQuickItem* drag_tab = tab_item(1);
        REQUIRE(drag_tab != nullptr);
        dock::DockPanel* dragged = shown.at(1);

        const QPoint press_pos = host_window.mapFromGlobal(
            drag_tab->mapToGlobal(QPointF(drag_tab->width() / 2, drag_tab->height() / 2)).toPoint());
        const QPoint drop_pos = host_window.mapFromGlobal(central_rect.center());

        QTest::mousePress(&host_window, Qt::LeftButton, Qt::KeyboardModifiers(), press_pos);
        QTest::qWait(20);
        QTest::mouseMove(&host_window, drop_pos, 20);
        QTest::qWait(20);
        QTest::mouseRelease(&host_window, Qt::LeftButton, Qt::KeyboardModifiers(), drop_pos);
        QTest::qWait(20);

        CHECK(drag.phase() == dock::DragSession::Phase::Idle);
        REQUIRE(dock::DockCatalog::self().windows().size() == 1);
        dock::DockWindow* floated = dock::DockCatalog::self().windows().first();
        REQUIRE(floated->group() != nullptr);
        CHECK(floated->group()->panels().contains(dragged));
        REQUIRE(drag.reattachGroup(floated->group()));
        QCoreApplication::processEvents();
        CHECK(dock::DockCatalog::self().windows().isEmpty());
    }

    // B. 横向拖动未激活标签 → 组内重排提交
    {
        const QList<dock::DockPanel*> before = panel_b->group()->shownPanels();
        REQUIRE(before.size() >= 2);
        panel_b->group()->setActivePanel(before.first());
        QCoreApplication::processEvents();

        REQUIRE(QQuickTest::qWaitForPolish(&host_window));
        QQuickItem* first = tab_item(0);
        QQuickItem* second = tab_item(1);
        REQUIRE(first != nullptr);
        REQUIRE(second != nullptr);

        const QPoint press_pos = host_window.mapFromGlobal(
            second->mapToGlobal(QPointF(second->width() / 2, second->height() / 2)).toPoint());
        const QPoint move_pos = host_window.mapFromGlobal(
            first->mapToGlobal(QPointF(first->width() * 0.25, first->height() / 2)).toPoint());

        QTest::mousePress(&host_window, Qt::LeftButton, Qt::KeyboardModifiers(), press_pos);
        QTest::qWait(20);
        QTest::mouseMove(&host_window, move_pos, 20);
        QTest::qWait(20);
        QTest::mouseRelease(&host_window, Qt::LeftButton, Qt::KeyboardModifiers(), move_pos);
        QTest::qWait(20);

        const QList<dock::DockPanel*> after = panel_b->group()->shownPanels();
        REQUIRE(after.size() == before.size());
        CHECK(after.at(0) == before.at(1));
        CHECK(after.at(1) == before.at(0));
        CHECK(dock::DockCatalog::self().windows().isEmpty());
    }

    // C. 标题行空白拖动整组 → 浮窗并回停
    {
        REQUIRE(QQuickTest::qWaitForPolish(&host_window));
        QQuickItem* buttons = merged_view->findChild<QQuickItem*>(QStringLiteral("titleButtons"));
        REQUIRE(buttons != nullptr);
        const qreal empty_x = (tab_bar->x() + tab_bar->width() + buttons->x()) / 2.0;
        REQUIRE(tab_bar->x() + tab_bar->width() < buttons->x());

        const QPoint press_pos = host_window.mapFromGlobal(
            merged_view->mapToGlobal(QPointF(1 + empty_x, 15)).toPoint());
        const QPoint drop_pos = host_window.mapFromGlobal(central_rect.center());

        QTest::mousePress(&host_window, Qt::LeftButton, Qt::KeyboardModifiers(), press_pos);
        QTest::qWait(20);
        QTest::mouseMove(&host_window, drop_pos, 20);
        QTest::qWait(20);
        QTest::mouseRelease(&host_window, Qt::LeftButton, Qt::KeyboardModifiers(), drop_pos);
        QTest::qWait(20);

        REQUIRE(dock::DockCatalog::self().windows().size() == 1);
        dock::DockWindow* floated = dock::DockCatalog::self().windows().first();
        CHECK(floated->group() == panel_b->group());
        REQUIRE(drag.reattachGroup(floated->group()));
        QCoreApplication::processEvents();
        CHECK(dock::DockCatalog::self().windows().isEmpty());
    }

    // 空浮窗自动回停：浮窗内隐藏全部面板后窗口销毁，重新显示回到停靠位置
    {
        dock::ui::PanelGroupItem* view = platform->panelGroupItem(panel_b->group());
        REQUIRE(view != nullptr);
        const QList<dock::DockPanel*> shown = panel_b->group()->shownPanels();
        REQUIRE_FALSE(shown.isEmpty());

        view->toggleDetached();
        QCoreApplication::processEvents();
        REQUIRE(dock::DockCatalog::self().windows().size() == 1);
        CHECK(panel_b->group()->vacancy() != nullptr);

        view->hideGroup();
        QCoreApplication::processEvents();
        CHECK(dock::DockCatalog::self().windows().isEmpty());
        CHECK(panel_b->group()->vacancy() == nullptr);

        for (dock::DockPanel* panel : shown)
            panel->showPanel();
        QCoreApplication::processEvents();
        REQUIRE(panel_b->group()->node() != nullptr);
        CHECK(panel_b->group()->node()->isVisible());
    }

    // 中央持久面板标签不可拖拽（拖拽入口守卫，不再产生浮窗）
    dock::ui::PanelGroupItem* central_view = platform->panelGroupItem(host->centralGroup());
    REQUIRE(central_view != nullptr);
    central_view->beginPanelDrag(0, QPoint(400, 300));
    drag.updateAt(QPoint(500, 300));
    CHECK(drag.phase() == dock::DragSession::Phase::Idle);
    CHECK(dock::DockCatalog::self().windows().isEmpty());

    // 布局持久化：QML 接口往返（保存 → 改动 → 恢复）
    {
        auto* host_item = qobject_cast<dock::ui::DockHostItem*>(root);
        REQUIRE(host_item != nullptr);

        const QList<dock::DockPanel*> shown_before = panel_b->group()->shownPanels();
        REQUIRE(shown_before.size() >= 2);
        const QString saved = host_item->saveLayout();
        REQUIRE_FALSE(saved.isEmpty());

        dock::DockPanel* hidden_target = shown_before.last();
        hidden_target->hidePanel();
        QCoreApplication::processEvents();
        CHECK_FALSE(hidden_target->isShown());

        CHECK(host_item->restoreLayout(saved));
        QCoreApplication::processEvents();
        CHECK(hidden_target->isShown());
        CHECK(panel_b->group()->shownPanels().size() == shown_before.size());
    }

    // 面板仍存活时先回收浮窗（析构守卫只作失败兜底）
    cleanup.clean();
    area_item->setParentItem(nullptr);
    delete root;
}

TEST_CASE("DockQml: object tree remains usable after small startup and layout restore")
{
    testApplication();
    docktest::DockSessionCleanup cleanup;
    QQmlEngine engine;
    dock::init(&engine);
    QQmlComponent component(&engine);
    component.setData(R"QML(
        import QtQuick
        import PreCess.Docking as Docking
        Docking.DockHost {
            id: host
            width: 800
            height: 360
            uniqueName: "smallStartup"
            centralItemFile: "qrc:/precess/dock/test/CentralStub.qml"
            Docking.DockPanel {
                id: tree
                uniqueName: "tree"
                Item { implicitWidth: 200; implicitHeight: 180; anchors.fill: parent }
            }
            Docking.DockPanel {
                id: operations
                uniqueName: "operations"
                Item { implicitWidth: 200; implicitHeight: 120; anchors.fill: parent }
            }
            Component.onCompleted: {
                host.placePanel(tree, Docking.Tokens.DockEdge.Left, null, Qt.size(250, 0))
                host.placePanel(operations, Docking.Tokens.DockEdge.Bottom, tree, Qt.size(0, 400))
            }
        }
    )QML",
        QUrl());
    INFO(component.errorString().toStdString());
    std::unique_ptr<QObject> root(component.create());
    REQUIRE(root != nullptr);
    auto* host_item = qobject_cast<dock::ui::DockHostItem*>(root.get());
    REQUIRE(host_item != nullptr);
    dock::DockPanel* tree = nullptr;
    for (auto* panel : dock::DockCatalog::self().panels()) {
        if (panel->uniqueName() == QStringLiteral("tree"))
            tree = panel;
    }
    REQUIRE(tree != nullptr);
    auto* client = dock::ui::DockRuntime::instance().panelContentItem(tree);
    REQUIRE(client != nullptr);
    CHECK(tree->group()->node()->geometry().height() >= 180);
    CHECK(client->height() > 100);

    // 小窗口完成初始化后最大化，关闭面板并恢复默认快照，内容仍可见可用。
    host_item->setHeight(900);
    const QString initial_layout = host_item->saveLayout();
    tree->hidePanel();
    CHECK_FALSE(tree->isShown());
    REQUIRE(host_item->restoreLayout(initial_layout));
    QCoreApplication::processEvents();
    CHECK(tree->isShown());
    CHECK(client->isVisible());
    CHECK(client->width() >= 198);
    CHECK(client->height() > 100);
}
