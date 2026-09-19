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
#include "ui/DropZoneOverlay.h"
#include "ui/DockRuntime.h"

#include <catch2/catch_test_macros.hpp>

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>

TEST_CASE("DockQml: docking area loads from QML and lays out docks")
{
    qputenv("QT_QPA_PLATFORM", "offscreen");

    int argc = 0;
    QGuiApplication app(argc, nullptr);

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

            Component.onCompleted: {
                area.placePanel(panelA, Docking.Tokens.DockEdge.Left, null, Qt.size(200, 0))
                area.placePanel(panelB, Docking.Tokens.DockEdge.Bottom, null, Qt.size(0, 150),
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
    QCoreApplication::processEvents();

    auto* platform = &dock::ui::DockRuntime::instance();

    // 基础：注册表与开关状态
    dock::DockHost* host = dock::DockCatalog::self().host();
    REQUIRE(host != nullptr);
    CHECK(dock::DockCatalog::self().panels().size() == 3); // panelA + panelB + 中央

    dock::DockPanel* panel_a = nullptr;
    dock::DockPanel* panel_b = nullptr;
    for (dock::DockPanel* panel : dock::DockCatalog::self().panels()) {
        if (panel->uniqueName() == QStringLiteral("panelA"))
            panel_a = panel;
        else if (panel->uniqueName() == QStringLiteral("panelB"))
            panel_b = panel;
    }
    REQUIRE(panel_a != nullptr);
    REQUIRE(panel_b != nullptr);
    CHECK(panel_a->isShown());
    CHECK_FALSE(panel_b->isShown());

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

    // 标签拖拽入口（QML 调用路径）：分离单个标签为浮窗并回停
    dock::ui::PanelGroupItem* merged_view = platform->panelGroupItem(panel_b->group());
    REQUIRE(merged_view != nullptr);
    merged_view->beginPanelDrag(0, target_center);
    drag.updateAt(target_center + QPoint(-80, -80));
    REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);
    CHECK(dock::DockCatalog::self().windows().size() == 1);
    drag.endAt(QPoint(-1000, -1000)); // 空白处释放：保留浮动
    CHECK(drag.phase() == dock::DragSession::Phase::Idle);
    REQUIRE(dock::DockCatalog::self().windows().size() == 1);
    {
        dock::DockWindow* separated = dock::DockCatalog::self().windows().first();
        REQUIRE(separated->group() != nullptr);
        CHECK(separated->group()->panels().contains(panel_b));
        CHECK_FALSE(separated->group()->panels().contains(panel_a));
        REQUIRE(drag.reattachGroup(separated->group()));
    }
    CHECK(dock::DockCatalog::self().windows().isEmpty());
    CHECK(panel_b->group() == panel_a->group());

    delete root;
}
