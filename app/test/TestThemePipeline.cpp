/**
 * @file TestThemePipeline.cpp
 * @brief 主题配色管道：同一份已解析角色表到达应用调色板与停靠主题
 *
 * 配色规则只在 app/Theme.qml 维护；本测试固定一份输入，验证两个接收端
 * 都按原值接收，避免再次出现跨模块各自计算配色。
 */
#include "core/AppPalette.h"
#include "ui/DockTheme.h"

#include <QColor>
#include <QGuiApplication>
#include <QPalette>
#include <QVariantMap>
#include <catch2/catch_test_macros.hpp>

namespace {

QGuiApplication& testApplication()
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    static int argc = 1;
    static char name[] = "TestThemePipeline";
    static char* argv[] = { name, nullptr };
    static QGuiApplication app(argc, argv);
    return app;
}

QVariantMap roleColors(bool dark, bool has_wallpaper)
{
    const QColor surface = has_wallpaper
        ? (dark ? QColor(37, 37, 38, 184) : QColor(255, 255, 255, 184))
        : (dark ? QColor("#252526") : QColor("#FFFFFF"));

    QVariantMap colors;
    colors.insert("window", dark ? QColor("#1E1E1E") : QColor("#F3F4F6"));
    colors.insert("windowText", dark ? QColor("#D4D4D4") : QColor("#2B2F36"));
    colors.insert("base", surface);
    colors.insert("alternateBase", dark ? QColor("#2D2D30") : QColor("#F7F8FA"));
    colors.insert("text", dark ? QColor("#D4D4D4") : QColor("#2B2F36"));
    colors.insert("button", dark ? QColor("#2D2D30") : QColor("#F7F8FA"));
    colors.insert("buttonText", dark ? QColor("#D4D4D4") : QColor("#2B2F36"));
    colors.insert("highlight", dark ? QColor("#4EA1F3") : QColor("#1976D2"));
    colors.insert("highlightedText", QColor("#FFFFFF"));
    colors.insert("placeholderText", dark ? QColor("#6B6F76") : QColor("#9CA3AF"));
    colors.insert("mid", dark ? QColor("#4A4A4F") : QColor("#C3C8CF"));
    colors.insert("midlight", dark ? QColor("#3E3E42") : QColor("#D9DDE3"));
    colors.insert("light", surface);
    colors.insert("dark", dark ? QColor("#4A4A4F") : QColor("#C3C8CF"));
    colors.insert("shadow", dark ? QColor("#5A5A5A") : QColor("#B7BDC7"));
    colors.insert("link", dark ? QColor("#4EA1F3") : QColor("#1976D2"));
    colors.insert("toolTipBase", dark ? QColor("#252526") : QColor("#FFFFFF"));
    colors.insert("toolTipText", dark ? QColor("#D4D4D4") : QColor("#2B2F36"));
    return colors;
}

QColor colorOf(const QVariantMap& colors, const char* key)
{
    return colors.value(QLatin1String(key)).value<QColor>();
}

}

TEST_CASE("One resolved role map reaches app palette and dock theme", "[ThemePipeline]")
{
    testApplication();

    const QVariantMap control = roleColors(true, true);
    const QVariantMap popup = roleColors(true, false);
    const QVariantMap dock_colors {
        { QStringLiteral("surface"), colorOf(control, "base") },
        { QStringLiteral("menuSurface"), colorOf(popup, "window") }
    };

    AppPalette app_palette;
    app_palette.setColors(control);

    dock::ui::DockTheme dock_theme;
    dock_theme.setControlPalette(control);
    dock_theme.setPopupPalette(popup);
    dock_theme.setColors(dock_colors);

    // 应用级调色板直接采用同一份角色表
    REQUIRE(QGuiApplication::palette().color(QPalette::Window) == colorOf(control, "window"));
    REQUIRE(QGuiApplication::palette().color(QPalette::Base) == colorOf(control, "base"));
    REQUIRE(QGuiApplication::palette().color(QPalette::ToolTipBase) == colorOf(control, "toolTipBase"));

    // 浮窗公共祖先与菜单消费的也是同一份结果，不再二次解析
    REQUIRE(dock_theme.controlPalette() == control);
    REQUIRE(dock_theme.popupPalette() == popup);
    REQUIRE(dock_theme.colors() == dock_colors);
}
