/**
 * @file TestThemeWindowPalette.cpp
 * @brief 主窗口窗口级调色板回归测试：后创建的 Fusion 控件继承 Theme 令牌
 *
 * 从生产 Main.qml 提取窗口调色板块，验证启动时创建与之后动态创建的下拉框
 * 都从窗口调色板继承主题色。Fusion 的默认控件调色板来自平台主题，若移除
 * 窗口级绑定，后创建控件会退回系统浅色。
 */
#include <QByteArray>
#include <QColor>
#include <QFile>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
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
    static char name[] = "TestThemeWindowPalette";
    static char* argv[] = { name, nullptr };
    static QGuiApplication app(argc, argv);
    return app;
}
}

TEST_CASE("Main window palette reaches controls created after startup", "[QML][theme]")
{
    testApplication();
    QQuickStyle::setStyle("Fusion");

    QFile main_qml(":/theme-window/Main.qml");
    REQUIRE(main_qml.open(QIODevice::ReadOnly));
    const QByteArray source = main_qml.readAll().replace("\r\n", "\n");

    // 提取生产窗口调色板块：连续以 palette. 开头的绑定行。
    QByteArray palette_bindings;
    int palette_role_count = 0;
    const QList<QByteArray> lines = source.split('\n');
    for (const QByteArray& line : lines) {
        if (!line.trimmed().startsWith("palette."))
            continue;
        palette_bindings += line + '\n';
        ++palette_role_count;
    }
    REQUIRE(palette_role_count >= 18);

    // 固定假角色表，避免测试依赖本机 QSettings 中的主题选择。
    const QColor surface("#112233");
    QVariantMap control_palette;
    control_palette.insert("window", QColor("#010203"));
    control_palette.insert("windowText", QColor("#040506"));
    control_palette.insert("base", surface);
    control_palette.insert("alternateBase", QColor("#070809"));
    control_palette.insert("text", QColor("#0a0b0c"));
    control_palette.insert("button", QColor("#0d0e0f"));
    control_palette.insert("buttonText", QColor("#101112"));
    control_palette.insert("highlight", QColor("#131415"));
    control_palette.insert("highlightedText", QColor("#161718"));
    control_palette.insert("placeholderText", QColor("#191a1b"));
    control_palette.insert("mid", QColor("#1c1d1e"));
    control_palette.insert("midlight", QColor("#1f2021"));
    control_palette.insert("light", QColor("#222324"));
    control_palette.insert("dark", QColor("#252627"));
    control_palette.insert("shadow", QColor("#28292a"));
    control_palette.insert("link", QColor("#2b2c2d"));
    control_palette.insert("toolTipBase", QColor("#2e2f30"));
    control_palette.insert("toolTipText", QColor("#313233"));

    QVariantMap theme_tokens;
    theme_tokens.insert("controlPalette", control_palette);

    QByteArray fixture = R"QML(
import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: root
    width: 200
    height: 200
    visible: true

)QML";
    fixture += palette_bindings;
    fixture += R"QML(
    ComboBox {
        id: earlyCombo
    }

    property var lateCombo: null
    property color earlyBase: earlyCombo.palette.base
    property color lateBase: lateCombo ? lateCombo.palette.base : "transparent"

    Item {
        id: lateHost

        Component.onCompleted: {
            root.lateCombo = Qt.createQmlObject('import QtQuick.Controls; ComboBox {}', lateHost)
            root.lateBase = root.lateCombo.palette.base
        }
    }
}
)QML";

    QQmlEngine engine;
    engine.rootContext()->setContextProperty("Theme", theme_tokens);
    QQmlComponent component(&engine);
    component.setData(fixture, QUrl());
    INFO(component.errorString().toStdString());
    std::unique_ptr<QObject> window(component.create());
    REQUIRE(window);
    QCoreApplication::processEvents();

    CHECK(window->property("earlyBase").value<QColor>() == surface);
    CHECK(window->property("lateBase").value<QColor>() == surface);
}
