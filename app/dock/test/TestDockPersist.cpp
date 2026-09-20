/**
 * @file TestDockPersist.cpp
 * @brief 布局持久化测试：快照往返、浮窗占位/回停来源、兼容性与坏数据拒绝
 */

#include "DockTestSupport.h"

#include "docking/DockCatalog.h"
#include "docking/DockHost.h"
#include "docking/DockPanel.h"
#include "docking/DockRegion.h"
#include "docking/DockWindow.h"
#include "docking/DragHandle.h"
#include "docking/DragSession.h"
#include "docking/PanelGroup.h"
#include "tree/BoxNode.h"
#include "tree/LayoutNode.h"

#include <catch2/catch_test_macros.hpp>

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <functional>

using namespace docktest;

namespace {

//! @brief 布局结构指纹：树序/占比/标签顺序与显隐/激活/浮窗几何
QString layoutFingerprint(dock::DockHost& host)
{
    QStringList parts;

    std::function<void(dock::LayoutNode*)> walk;
    walk = [&parts, &walk](dock::LayoutNode* item) {
        if (!item)
            return;

        if (item->isContainer()) {
            auto* box = static_cast<dock::BoxNode*>(item);
            QStringList children;
            for (dock::LayoutNode* child : box->children()) {
                const double share = child->isVisible() ? child->share() : child->rememberedShare();
                children << QStringLiteral("%1@%2").arg(share, 0, 'f', 3);
                walk(child);
            }
            parts << QStringLiteral("box(%1)[%2]").arg(
                box->orientation() == Qt::Horizontal ? QStringLiteral("h") : QStringLiteral("v"),
                children.join(QLatin1Char(',')));
            return;
        }

        if (auto* group = dynamic_cast<dock::PanelGroup*>(item->client())) {
            QStringList tabs;
            for (dock::DockPanel* panel : group->panels()) {
                tabs << QStringLiteral("%1%2").arg(panel->uniqueName(),
                    panel->isShown() ? QStringLiteral("+") : QStringLiteral("-"));
            }
            parts << QStringLiteral("group[%1|active=%2]").arg(tabs.join(QLatin1Char(',')),
                group->activePanel() ? group->activePanel()->uniqueName() : QStringLiteral("-"));
            return;
        }

        parts << QStringLiteral("vacancy");
    };

    if (dock::DockRegion* region = host.region()) {
        walk(region->rootNode());
    }

    for (dock::DockWindow* window : dock::DockCatalog::self().windows()) {
        const QRect geometry = window->geometry();
        parts << QStringLiteral("window(%1,%2,%3,%4){").arg(geometry.x()).arg(geometry.y())
                     .arg(geometry.width()).arg(geometry.height());
        walk(window->region()->rootNode());
        parts << QStringLiteral("}");
    }

    return parts.join(QLatin1Char('|'));
}

//! @brief 通过浮窗内的面板定位承载它的浮窗
dock::DockWindow* windowForPanel(dock::DockPanel* panel)
{
    for (dock::DockWindow* window : dock::DockCatalog::self().windows()) {
        if (window->region() && panel->isDetached()
            && panel->group() && window->region()->groups().contains(panel->group()))
            return window;
    }
    return nullptr;
}

} // namespace

TEST_CASE("DockPersist: save and restore round-trips layout with floats")
{
    DockFixture f;
    PlacedDocks placed(f);
    dock::DragSession& drag = dock::DragSession::self();

    placed.object_tree->showPanel();
    placed.side_bar->showPanel();
    placed.console->showPanel();

    // sideBar 合并为 objectTree 分组的第二个标签（走合并删除源分组）
    REQUIRE(drag.detachGroup(placed.side_bar->group()));
    {
        dock::DockWindow* window = dock::DockCatalog::self().windows().first();
        dock::DragHandle handle(nullptr, placed.side_bar->group(), window);
        const QRect target = placed.object_tree->group()->node()->geometry();
        drag.beginAt(&handle, target.center() + QPoint(-50, -50));
        drag.updateAt(target.center());
        REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);
        drag.endAt(target.center());
    }
    REQUIRE(placed.side_bar->group() == placed.object_tree->group());
    placed.side_bar->showPanel();
    REQUIRE(placed.object_tree->group()->shownPanels().size() == 2);

    // 整组浮出 console（带占位）
    REQUIRE(drag.detachGroup(placed.console->group()));
    // 单标签浮出 sideBar（保留回停来源）
    {
        const QPoint press = placed.object_tree->group()->node()->geometry().center();
        dock::DragHandle handle(nullptr, placed.object_tree->group(), nullptr, placed.side_bar);
        drag.beginAt(&handle, press);
        drag.updateAt(press + QPoint(60, 0));
        REQUIRE(drag.phase() == dock::DragSession::Phase::Dragging);
        drag.endAt(QPoint(-1000, -1000));
    }
    REQUIRE(dock::DockCatalog::self().windows().size() == 2);

    const QByteArray saved = f.host.saveLayout();
    REQUIRE_FALSE(saved.isEmpty());
    const QString before = layoutFingerprint(f.host);

    // 全清后恢复
    f.host.clearLayout();
    CHECK(dock::DockCatalog::self().windows().isEmpty());
    CHECK(f.host.centralPanel()->isShown());
    CHECK(f.host.region()->groups().size() == 1); // 仅中央

    REQUIRE(f.host.restoreLayout(saved));
    const QString after = layoutFingerprint(f.host);
    INFO("before: " << before.toStdString() << "\nafter:  " << after.toStdString());
    CHECK(after == before);
    REQUIRE(dock::DockCatalog::self().windows().size() == 2);

    dock::DockWindow* console_window = windowForPanel(placed.console);
    dock::DockWindow* side_bar_window = windowForPanel(placed.side_bar);
    REQUIRE(console_window != nullptr);
    REQUIRE(side_bar_window != nullptr);

    // 整组浮窗：回停回到占位原位置
    REQUIRE(drag.reattachGroup(console_window->group()));
    CHECK(placed.console->isShown());
    CHECK_FALSE(placed.console->isDetached());

    // 单标签浮窗：回停归还源分组
    REQUIRE(drag.reattachGroup(side_bar_window->group()));
    CHECK(placed.side_bar->group() == placed.object_tree->group());
    CHECK_FALSE(placed.side_bar->isDetached());
    CHECK(dock::DockCatalog::self().windows().isEmpty());

#ifdef QT_DEBUG
    f.host.region()->validateTree();
#endif
}

TEST_CASE("DockPersist: tolerates unknown panels and rejects bad data")
{
    DockFixture f;
    PlacedDocks placed(f);
    placed.console->showPanel();

    const QByteArray saved = f.host.saveLayout();

    // 快照中的 console 替换为未知面板名：跳过未知项，面板按原显隐兜底放置
    QString text = QString::fromUtf8(saved);
    text.replace(QStringLiteral("console"), QStringLiteral("ghost"));
    REQUIRE(f.host.restoreLayout(text.toUtf8()));
    CHECK(placed.console->isShown());
    REQUIRE(placed.console->group() != nullptr);
    CHECK(placed.console->group()->panels().contains(placed.console));

    // 坏数据/版本/宿主不匹配：拒绝且不改动布局
    const QString fingerprint = layoutFingerprint(f.host);
    CHECK_FALSE(f.host.restoreLayout(QByteArray()));
    CHECK_FALSE(f.host.restoreLayout(QByteArray("{not json")));

    QJsonObject bad_version = QJsonDocument::fromJson(saved).object();
    bad_version.insert(QStringLiteral("version"), 99);
    CHECK_FALSE(
        f.host.restoreLayout(QJsonDocument(bad_version).toJson(QJsonDocument::Compact)));

    QJsonObject bad_host = QJsonDocument::fromJson(saved).object();
    bad_host.insert(QStringLiteral("host"), QStringLiteral("OtherLayout"));
    CHECK_FALSE(f.host.restoreLayout(QJsonDocument(bad_host).toJson(QJsonDocument::Compact)));

    CHECK(layoutFingerprint(f.host) == fingerprint);
}

TEST_CASE("DockPersist: panels added after snapshot survive restore")
{
    DockFixture f;
    PlacedDocks placed(f);
    placed.console->showPanel();

    const QByteArray saved = f.host.saveLayout();

    dock::DockPanel* extra = f.makeDock(QStringLiteral("late"), QStringLiteral("后加面板"));
    f.host.placePanel(extra, dock::DockEdge::Right, nullptr, QSize(120, 0));
    REQUIRE(extra->isShown());

    REQUIRE(f.host.restoreLayout(saved));

    // 后加面板不在快照中：按原有显隐兜底放置，仍可用
    CHECK(extra->isShown());
    REQUIRE(extra->group() != nullptr);
    CHECK(extra->group()->panels().contains(extra));
    // 快照中的面板保持原有可见性
    CHECK(placed.console->isShown());
    CHECK(placed.object_tree->isShown());
}

TEST_CASE("DockPersist: clearLayout keeps central and drops floats")
{
    DockFixture f;
    PlacedDocks placed(f);
    dock::DragSession& drag = dock::DragSession::self();

    placed.console->showPanel();
    REQUIRE(drag.detachGroup(placed.console->group()));
    REQUIRE(dock::DockCatalog::self().windows().size() == 1);

    f.host.clearLayout();

    CHECK(dock::DockCatalog::self().windows().isEmpty());
    CHECK(f.host.centralPanel()->isShown());
    CHECK_FALSE(f.host.centralPanel()->isDetached());
    CHECK(f.host.region()->groups().size() == 1);
    CHECK(f.host.region()->rootNode() == f.host.centralNode());
    CHECK_FALSE(placed.console->isShown());
    CHECK(placed.console->group() == nullptr);
}
