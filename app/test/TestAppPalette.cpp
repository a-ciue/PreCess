/**
 * @file TestAppPalette.cpp
 * @brief 主题调色板适配测试：角色表映射、alpha 保留与应用级调色板刷新
 */
#include "core/AppPalette.h"
#include "core/ThemePalette.h"

#include <QColor>
#include <QGuiApplication>
#include <QPalette>
#include <QVariantMap>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace {

QGuiApplication& testApplication()
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    static int argc = 1;
    static char name[] = "TestAppPalette";
    static char* argv[] = { name, nullptr };
    static QGuiApplication app(argc, argv);
    return app;
}

//! @brief 构造带 alpha 的期望色
QColor withAlpha(const char* hex, qreal alpha)
{
    QColor color { QLatin1String(hex) };
    color.setAlphaF(alpha);
    return color;
}

//! @brief Theme.controlPalette 形状的角色表（值仅用于断言唯一映射）
QVariantMap roleColors()
{
    QVariantMap colors;
    colors.insert("window", QColor("#010203"));
    colors.insert("windowText", QColor("#040506"));
    colors.insert("base", withAlpha("#070809", 0.72));
    colors.insert("alternateBase", withAlpha("#0a0b0c", 0.62));
    colors.insert("text", QColor("#0d0e0f"));
    colors.insert("button", QColor("#101112"));
    colors.insert("buttonText", QColor("#131415"));
    colors.insert("highlight", QColor("#161718"));
    colors.insert("highlightedText", QColor("#191a1b"));
    colors.insert("placeholderText", QColor("#1c1d1e"));
    colors.insert("mid", QColor("#1f2021"));
    colors.insert("midlight", QColor("#222324"));
    colors.insert("light", QColor("#252627"));
    colors.insert("dark", QColor("#28292a"));
    colors.insert("shadow", QColor("#2b2c2d"));
    colors.insert("link", QColor("#2e2f30"));
    colors.insert("toolTipBase", QColor("#313233"));
    colors.insert("toolTipText", QColor("#343536"));
    return colors;
}

QColor colorOf(const QVariantMap& colors, const char* key)
{
    return colors.value(QLatin1String(key)).value<QColor>();
}

}

TEST_CASE("ThemePalette: resolved role colors map to QPalette", "[ThemePalette]")
{
    const QVariantMap colors = roleColors();
    const QPalette palette = app::core::buildThemePalette(colors);

    REQUIRE(palette.color(QPalette::Window) == colorOf(colors, "window"));
    REQUIRE(palette.color(QPalette::WindowText) == colorOf(colors, "windowText"));
    REQUIRE(palette.color(QPalette::Base) == colorOf(colors, "base"));
    REQUIRE(palette.color(QPalette::AlternateBase) == colorOf(colors, "alternateBase"));
    REQUIRE(palette.color(QPalette::Text) == colorOf(colors, "text"));
    REQUIRE(palette.color(QPalette::Button) == colorOf(colors, "button"));
    REQUIRE(palette.color(QPalette::ButtonText) == colorOf(colors, "buttonText"));
    REQUIRE(palette.color(QPalette::Highlight) == colorOf(colors, "highlight"));
    REQUIRE(palette.color(QPalette::HighlightedText) == colorOf(colors, "highlightedText"));
    REQUIRE(palette.color(QPalette::PlaceholderText) == colorOf(colors, "placeholderText"));
    REQUIRE(palette.color(QPalette::Mid) == colorOf(colors, "mid"));
    REQUIRE(palette.color(QPalette::Midlight) == colorOf(colors, "midlight"));
    REQUIRE(palette.color(QPalette::Light) == colorOf(colors, "light"));
    REQUIRE(palette.color(QPalette::Dark) == colorOf(colors, "dark"));
    REQUIRE(palette.color(QPalette::Shadow) == colorOf(colors, "shadow"));
    REQUIRE(palette.color(QPalette::Link) == colorOf(colors, "link"));
    REQUIRE(palette.color(QPalette::ToolTipBase) == colorOf(colors, "toolTipBase"));
    REQUIRE(palette.color(QPalette::ToolTipText) == colorOf(colors, "toolTipText"));

    // 半透明面色由 Theme 预先解析，适配层原样保留 alpha
    REQUIRE(palette.color(QPalette::Base).alphaF() == Catch::Approx(0.72).epsilon(0.001));
    REQUIRE(palette.color(QPalette::AlternateBase).alphaF() == Catch::Approx(0.62).epsilon(0.001));
}

TEST_CASE("AppPalette: applies resolved roles to the application palette", "[AppPalette]")
{
    testApplication();

    const QVariantMap colors = roleColors();
    AppPalette app_palette;
    app_palette.setColors(colors);

    REQUIRE(app_palette.colors() == colors);
    REQUIRE(QGuiApplication::palette().color(QPalette::Window) == colorOf(colors, "window"));
    REQUIRE(QGuiApplication::palette().color(QPalette::Base) == colorOf(colors, "base"));
    REQUIRE(QGuiApplication::palette().color(QPalette::ToolTipBase) == colorOf(colors, "toolTipBase"));
}
