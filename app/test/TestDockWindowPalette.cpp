/**
 * @file TestDockWindowPalette.cpp
 * @brief 浮窗公共祖先调色板回归测试：后创建的控件继承宿主推送的主题
 *
 * 加载生产 DockWindow.qml，向 Docking.Theme 推送普通控件调色板，验证浮窗根项
 * 内部创建的 ComboBox 继承角色颜色；切换配色后既有控件与随后创建的控件同步更新。
 */
#include "Docking.h"

#include <QColor>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QVariantMap>
#include <catch2/catch_test_macros.hpp>
#include <memory>

namespace {

QGuiApplication& testApplication()
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    static int argc = 1;
    static char name[] = "TestDockWindowPalette";
    static char* argv[] = { name, nullptr };
    static QGuiApplication app(argc, argv);
    return app;
}

QVariantMap controlPalette(const QColor& base, const QColor& tool_tip_base)
{
    QVariantMap palette;
    palette.insert("window", QColor("#010203"));
    palette.insert("windowText", QColor("#040506"));
    palette.insert("base", base);
    palette.insert("alternateBase", QColor("#070809"));
    palette.insert("text", QColor("#0a0b0c"));
    palette.insert("button", QColor("#0d0e0f"));
    palette.insert("buttonText", QColor("#101112"));
    palette.insert("highlight", QColor("#131415"));
    palette.insert("highlightedText", QColor("#161718"));
    palette.insert("placeholderText", QColor("#191a1b"));
    palette.insert("mid", QColor("#1c1d1e"));
    palette.insert("midlight", QColor("#1f2021"));
    palette.insert("light", QColor("#222324"));
    palette.insert("dark", QColor("#252627"));
    palette.insert("shadow", QColor("#28292a"));
    palette.insert("link", QColor("#2b2c2d"));
    palette.insert("toolTipBase", tool_tip_base);
    palette.insert("toolTipText", QColor("#313233"));
    return palette;
}

//! @brief 创建读取自身控件调色板的宿主；ComboBox 在挂到浮窗后再创建
QQuickItem* createPaletteProbe(QQmlEngine& engine, QQuickItem* parent)
{
    QQmlComponent component(&engine);
    component.setData(R"QML(
import QtQuick
import QtQuick.Controls

Item {
    id: host
    property var combo: null
    property color inheritedBase: combo ? combo.palette.base : "transparent"
    property color inheritedToolTipBase: combo ? combo.palette.toolTipBase : "transparent"

    function createCombo() {
        combo = Qt.createQmlObject('import QtQuick.Controls; ComboBox {}', host)
    }
}
)QML",
        QUrl());
    INFO(component.errorString().toStdString());
    auto* probe = qobject_cast<QQuickItem*>(component.create());
    REQUIRE(probe);
    probe->setParentItem(parent);
    REQUIRE(QMetaObject::invokeMethod(probe, "createCombo"));
    return probe;
}

}

TEST_CASE("Floating dock window content inherits pushed control palette", "[QML][DockTheme]")
{
    testApplication();
    QQuickStyle::setStyle("Fusion");

    QQmlEngine engine;
    dock::init(&engine);

    auto* dock_theme = engine.singletonInstance<QObject*>("PreCess.Docking", "Theme");
    REQUIRE(dock_theme);

    const QColor initial_base("#112233");
    const QColor initial_tooltip("#445566");
    const QColor updated_base("#778899");

    dock_theme->setProperty("controlPalette", controlPalette(initial_base, initial_tooltip));
    dock_theme->setProperty("popupPalette", QVariantMap { { QStringLiteral("window"), initial_tooltip } });
    dock_theme->setProperty("colors", QVariantMap { { QStringLiteral("windowBackground"), QColor("#aabbcc") } });
    REQUIRE(dock_theme->property("controlPalette").value<QVariantMap>().value("base").value<QColor>()
        == initial_base);

    QQmlComponent dock_component(&engine, QUrl(QStringLiteral("qrc:/precess/dock/DockWindow.qml")));
    INFO(dock_component.errorString().toStdString());
    REQUIRE(dock_component.isReady());
    std::unique_ptr<QObject> dock_object(dock_component.create());
    REQUIRE(dock_object);
    auto* dock_root = qobject_cast<QQuickItem*>(dock_object.get());
    REQUIRE(dock_root);

    QCoreApplication::processEvents();

    std::unique_ptr<QQuickItem> early_probe(createPaletteProbe(engine, dock_root));
    QCoreApplication::processEvents();
    REQUIRE(early_probe->property("inheritedBase").value<QColor>() == initial_base);
    REQUIRE(early_probe->property("inheritedToolTipBase").value<QColor>() == initial_tooltip);

    // 运行期切换主题：既有控件更新
    dock_theme->setProperty("controlPalette", controlPalette(updated_base, initial_tooltip));
    QCoreApplication::processEvents();
    REQUIRE(early_probe->property("inheritedBase").value<QColor>() == updated_base);

    // 随后创建的控件走同一继承路径
    std::unique_ptr<QQuickItem> late_probe(createPaletteProbe(engine, dock_root));
    QCoreApplication::processEvents();
    REQUIRE(late_probe->property("inheritedBase").value<QColor>() == updated_base);
    REQUIRE(late_probe->property("inheritedToolTipBase").value<QColor>() == initial_tooltip);
}
