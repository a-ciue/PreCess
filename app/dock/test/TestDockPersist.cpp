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
#include <QJsonArray>
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

TEST_CASE("DockPersist: unknown-only group keeps later origin references safe")
{
    DockFixture f;
    PlacedDocks placed(f);
    placed.console->showPanel();

    const QString central_name = f.host.centralPanel()->uniqueName();

    // 主树：中央分组 + 幽灵分组（面板名未知，建组后即回收）
    QJsonObject central_group {
        { "type", "group" },
        { "id", 0 },
        { "panels", QJsonArray { QJsonObject { { "name", central_name }, { "shown", true } } } }
    };
    QJsonObject ghost_group {
        { "type", "group" },
        { "id", 2 },
        { "panels", QJsonArray { QJsonObject { { "name", "ghostA" }, { "shown", true } } } }
    };
    QJsonObject main_box {
        { "type", "box" },
        { "orientation", "horizontal" },
        { "children", QJsonArray {
            QJsonObject { { "share", 0.7 }, { "node", central_group } },
            QJsonObject { { "share", 0.3 }, { "node", ghost_group } } } }
    };
    QJsonObject window_tree {
        { "type", "group" },
        { "id", 1 },
        { "panels", QJsonArray { QJsonObject { { "name", "console" }, { "shown", true } } } }
    };
    QJsonObject window_entry {
        { "geometry", QJsonArray { 120, 120, 320, 220 } },
        { "primary", 1 },
        { "placeholder", false },
        { "tree", window_tree },
        // 回停来源指向被回收的幽灵分组 id：不得建成悬空引用
        { "origin", QJsonObject { { "group", 2 }, { "index", 0 } } }
    };
    QJsonObject root {
        { "version", 1 },
        { "host", "PreCessMainLayout" },
        { "central", central_name },
        { "main", main_box },
        { "windows", QJsonArray { window_entry } }
    };

    REQUIRE(f.host.restoreLayout(QJsonDocument(root).toJson(QJsonDocument::Compact)));
    REQUIRE(dock::DockCatalog::self().windows().size() == 1);
    dock::DockWindow* window = dock::DockCatalog::self().windows().first();
    REQUIRE(window->group() != nullptr);

    dock::PanelGroup* origin = nullptr;
    int index = -1;
    CHECK_FALSE(dock::DragSession::self().parkedOrigin(window->group(), origin, index));

    // 无占位/来源的浮窗分组经兜底停靠回主区域
    REQUIRE(dock::DragSession::self().reattachGroup(window->group()));
    CHECK(placed.console->isShown());
    CHECK(f.host.region()->groups().contains(placed.console->group()));
    CHECK(dock::DockCatalog::self().windows().isEmpty());

#ifdef QT_DEBUG
    f.host.region()->validateTree();
#endif
}

TEST_CASE("DockPersist: central panel outside the main tree is rejected")
{
    DockFixture f;
    PlacedDocks placed(f);
    placed.console->showPanel();

    const QString before = layoutFingerprint(f.host);
    const QString central_name = f.host.centralPanel()->uniqueName();

    QJsonObject console_group {
        { "type", "group" },
        { "id", 0 },
        { "panels", QJsonArray { QJsonObject { { "name", "console" }, { "shown", true } } } }
    };
    QJsonObject central_group {
        { "type", "group" },
        { "id", 1 },
        { "panels", QJsonArray { QJsonObject { { "name", central_name }, { "shown", true } } } }
    };
    QJsonObject window_entry {
        { "geometry", QJsonArray { 100, 100, 300, 200 } },
        { "primary", 1 },
        { "placeholder", false },
        { "tree", central_group }
    };
    QJsonObject root {
        { "version", 1 },
        { "host", "PreCessMainLayout" },
        { "central", central_name },
        { "main", console_group },
        { "windows", QJsonArray { window_entry } }
    };

    CHECK_FALSE(f.host.restoreLayout(QJsonDocument(root).toJson(QJsonDocument::Compact)));
    CHECK(layoutFingerprint(f.host) == before);
    CHECK(f.host.centralPanel()->isShown());
}

TEST_CASE("DockPersist: window primary pointing outside its tree is cleared")
{
    DockFixture f;
    PlacedDocks placed(f);
    placed.console->showPanel();

    const QString central_name = f.host.centralPanel()->uniqueName();
    QJsonObject main_group {
        { "type", "group" },
        { "id", 0 },
        { "panels", QJsonArray {
            QJsonObject { { "name", central_name }, { "shown", true } },
            QJsonObject { { "name", "console" }, { "shown", true } } } }
    };
    QJsonObject window_group {
        { "type", "group" },
        { "id", 1 },
        { "panels", QJsonArray { QJsonObject { { "name", "sideBar" }, { "shown", true } } } }
    };
    QJsonObject window_entry {
        { "geometry", QJsonArray { 140, 140, 300, 200 } },
        { "primary", 0 }, // 指向主树分组：非法，不得让窗口摘走主区域节点
        { "placeholder", false },
        { "tree", window_group }
    };
    QJsonObject root {
        { "version", 1 },
        { "host", "PreCessMainLayout" },
        { "central", central_name },
        { "main", main_group },
        { "windows", QJsonArray { window_entry } }
    };

    REQUIRE(f.host.restoreLayout(QJsonDocument(root).toJson(QJsonDocument::Compact)));
    REQUIRE(dock::DockCatalog::self().windows().size() == 1);
    dock::DockWindow* window = dock::DockCatalog::self().windows().first();
    CHECK(window->group() == nullptr);
    CHECK(f.host.region()->groups().contains(f.host.centralGroup()));
    CHECK(placed.console->group() == f.host.centralGroup());

    // 浮窗内的次级分组仍可回停主区域
    REQUIRE(dock::DragSession::self().reattachGroup(placed.side_bar->group()));
    CHECK(dock::DockCatalog::self().windows().isEmpty());
    CHECK(f.host.region()->groups().contains(placed.side_bar->group()));

#ifdef QT_DEBUG
    f.host.region()->validateTree();
#endif
}

TEST_CASE("DockPersist: absurd floating window geometry is rejected")
{
    DockFixture f;
    PlacedDocks placed(f);
    placed.console->showPanel();

    dock::DragSession& drag = dock::DragSession::self();
    REQUIRE(drag.detachGroup(placed.console->group()));
    REQUIRE(dock::DockCatalog::self().windows().size() == 1);

    const QByteArray saved = f.host.saveLayout();
    const QString before = layoutFingerprint(f.host);

    const auto with_geometry = [&saved](int x, int y, int w, int h) {
        QJsonObject root = QJsonDocument::fromJson(saved).object();
        QJsonArray windows = root.value(QStringLiteral("windows")).toArray();
        REQUIRE(windows.size() == 1);
        QJsonObject window = windows.at(0).toObject();
        window.insert(QStringLiteral("geometry"), QJsonArray { x, y, w, h });
        windows.replace(0, window);
        root.insert(QStringLiteral("windows"), windows);
        return QJsonDocument(root).toJson(QJsonDocument::Compact);
    };

    // 非正宽高与超上限边长都必须整快照拒绝，且不改动当前布局
    CHECK_FALSE(f.host.restoreLayout(with_geometry(100, 100, 0, 200)));
    CHECK_FALSE(f.host.restoreLayout(with_geometry(100, 100, 300, -20)));
    CHECK_FALSE(f.host.restoreLayout(with_geometry(100, 100, 40000, 200)));
    CHECK(layoutFingerprint(f.host) == before);
    CHECK(dock::DockCatalog::self().windows().size() == 1);

    REQUIRE(drag.reattachGroup(placed.console->group()));
    CHECK(dock::DockCatalog::self().windows().isEmpty());
}