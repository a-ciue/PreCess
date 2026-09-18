/**
 * @file TestDockQmlSmoke.cpp
 * @brief QML 集成冒烟测试：类型注册、中央持久部件、重开面板、拖拽浮窗回归
 *
 * 使用 offscreen 平台运行，覆盖：
 * - PreCess.Docking 类型注册与 Enums 枚举访问；
 * - persistentCentralItemFileName 加载与 guest 挂载（中央渲染窗口问题回归）；
 * - 面板关闭后重开 guest 恢复可见（“重开空白”问题回归）；
 * - 真实浮窗视图下的浮动 → 拖拽合并（“拖动崩溃”问题回归）。
 */

#include "Docking.h"

#include "core/DockRegistry.h"
#include "core/DockWidget.h"
#include "core/DragController.h"
#include "core/Draggable.h"
#include "core/DropArea.h"
#include "core/FloatingWindow.h"
#include "core/Group.h"
#include "core/MainWindow.h"
#include "core/View.h"
#include "engine/Item.h"
#include "qtquick/GroupView.h"
#include "qtquick/IndicatorsOverlayWindow.h"
#include "qtquick/Platform.h"

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

        Docking.DockingArea {
            id: area
            width: 800
            height: 600
            uniqueName: "smoke"
            options: Docking.Enums.MainWindowOption_HasCentralWidget
            persistentCentralItemFileName: "qrc:/precess/dock/test/CentralStub.qml"

            Docking.DockWidget {
                id: panelA
                uniqueName: "panelA"
                title: "Panel A"
                Item { anchors.fill: parent }
            }

            Docking.DockWidget {
                id: panelB
                uniqueName: "panelB"
                title: "Panel B"
                Item { anchors.fill: parent }
            }

            Component.onCompleted: {
                area.addDockWidget(panelA, Docking.Enums.Location_OnLeft, null, Qt.size(200, 0))
                area.addDockWidget(panelB, Docking.Enums.Location_OnBottom, null, Qt.size(0, 150),
                                   Docking.Enums.StartHidden)
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

    auto* platform = &dock::qtquick::Platform::instance();

    // 基础：注册表与开关状态
    dock::MainWindow* main_window = dock::DockRegistry::self().mainWindow();
    REQUIRE(main_window != nullptr);
    CHECK(dock::DockRegistry::self().dockWidgets().size() == 3); // panelA + panelB + 中央

    dock::DockWidget* panel_a = nullptr;
    dock::DockWidget* panel_b = nullptr;
    for (dock::DockWidget* dock_widget : dock::DockRegistry::self().dockWidgets()) {
        if (dock_widget->uniqueName() == QStringLiteral("panelA"))
            panel_a = dock_widget;
        else if (dock_widget->uniqueName() == QStringLiteral("panelB"))
            panel_b = dock_widget;
    }
    REQUIRE(panel_a != nullptr);
    REQUIRE(panel_b != nullptr);
    CHECK(panel_a->isOpen());
    CHECK_FALSE(panel_b->isOpen());

    // 布局：左侧面板 200 宽；隐藏面板不占空间
    dock::Item* panel_a_item = panel_a->group()->layoutItem();
    REQUIRE(panel_a_item != nullptr);
    CHECK(panel_a_item->isVisible());
    CHECK(panel_a_item->geometry().width() == 200);
    CHECK(panel_a_item->geometry().height() == 600);
    CHECK_FALSE(panel_b->group()->layoutItem()->isVisible());

    // 中央持久部件：guest 已挂载且可见
    dock::DockWidget* central = main_window->centralDockWidget();
    REQUIRE(central != nullptr);
    QQuickItem* central_guest = platform->guestItem(central);
    REQUIRE(central_guest != nullptr);
    CHECK(central_guest->isVisible());
    CHECK(central_guest->width() > 0);
    CHECK(central_guest->height() > 0);
    REQUIRE(central_guest->parentItem() != nullptr);
    CHECK(central_guest->parentItem()->objectName() == QStringLiteral("contentArea"));

    // 重开面板：guest 应恢复可见（重开空白回归）
    QQuickItem* panel_a_guest = platform->guestItem(panel_a);
    REQUIRE(panel_a_guest != nullptr);
    panel_a->close();
    QCoreApplication::processEvents();
    CHECK_FALSE(panel_a_guest->isVisible());
    panel_a->open();
    QCoreApplication::processEvents();
    CHECK(panel_a_guest->isVisible());

    // 打开底部面板，随后拖动合并（真实浮窗视图下的崩溃回归）
    panel_b->open();
    QCoreApplication::processEvents();
    CHECK(panel_b->group()->layoutItem()->isVisible());

    dock::DragController& drag = dock::DragController::self();
    REQUIRE(drag.floatGroup(panel_a->group()));
    QCoreApplication::processEvents();

    REQUIRE(dock::DockRegistry::self().floatingWindows().size() == 1);
    dock::FloatingWindow* floating_window = dock::DockRegistry::self().floatingWindows().first();
    CHECK(panel_a->isFloating());
    CHECK(floating_window->group() == panel_a->group());
    // 浮窗视图在首个有效几何到达后自行显示（无首帧空白）
    REQUIRE(floating_window->view() != nullptr);
    CHECK(floating_window->view()->isViewVisible());

    dock::Draggable draggable(nullptr, panel_a->group(), floating_window);
    const QRect target_rect = panel_b->group()->layoutItem()->geometry();
    const QPoint target_center = target_rect.center();

    dock::qtquick::IndicatorsOverlayWindow* overlay = platform->indicatorsOverlay();
    REQUIRE(overlay != nullptr);
    CHECK_FALSE(overlay->isActive());

    drag.onPress(&draggable, target_center + QPoint(-50, -50));
    drag.onMove(target_center + QPoint(-40, -40));
    REQUIRE(drag.state() == dock::DragController::State::Dragging);
    // 落点高亮由顶层浮层窗口呈现（不被拖动中的浮窗遮挡）
    CHECK(overlay->isActive());
    CHECK(overlay->isVisible());

    drag.onRelease(target_center);
    QCoreApplication::processEvents();

    CHECK(drag.state() == dock::DragController::State::Idle);
    CHECK(panel_a->group() == panel_b->group());
    CHECK_FALSE(panel_a->isFloating());
    CHECK(dock::DockRegistry::self().floatingWindows().isEmpty());
    CHECK_FALSE(overlay->isActive());

    // 合并后成为选项卡：切换为当前页后 guest 可见，且无悬空视图
    CHECK(panel_b->group()->dockWidgets().contains(panel_a));
    QQuickItem* merged_guest = platform->guestItem(panel_a);
    REQUIRE(merged_guest != nullptr);
    panel_b->group()->setCurrentDockWidget(panel_a);
    QCoreApplication::processEvents();
    CHECK(merged_guest->isVisible());

    // 标签拖拽入口（QML 调用路径）：分离单个标签为浮窗并回停
    dock::qtquick::GroupView* merged_view = platform->groupView(panel_b->group());
    REQUIRE(merged_view != nullptr);
    merged_view->beginTabDrag(0, target_center);
    drag.onMove(target_center + QPoint(-80, -80));
    REQUIRE(drag.state() == dock::DragController::State::Dragging);
    CHECK(dock::DockRegistry::self().floatingWindows().size() == 1);
    drag.onRelease(QPoint(-1000, -1000)); // 空白处释放：保留浮动
    CHECK(drag.state() == dock::DragController::State::Idle);
    REQUIRE(dock::DockRegistry::self().floatingWindows().size() == 1);
    {
        dock::FloatingWindow* separated = dock::DockRegistry::self().floatingWindows().first();
        REQUIRE(separated->group() != nullptr);
        CHECK(separated->group()->dockWidgets().contains(panel_b));
        CHECK_FALSE(separated->group()->dockWidgets().contains(panel_a));
        REQUIRE(drag.dockGroup(separated->group()));
    }
    CHECK(dock::DockRegistry::self().floatingWindows().isEmpty());
    CHECK(panel_b->group() == panel_a->group());

    delete root;
}
