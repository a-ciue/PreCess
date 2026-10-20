/**
 * @file TestThemeConsistency.cpp
 * @brief 主题色跨文件一致性：ThemePalette（应用调色板）与 DockTheme（停靠配色）
 *
 * 两处 C++ 各自维护浅/深色对，此测试断言重叠语义完全相等，防止手动同步漂移。
 * （app/Theme.qml 为第三份，属 QML 侧，不在本测试范围内。）
 */
#include "core/ThemePalette.h"
#include "ui/DockTheme.h"

#include <QColor>
#include <QPalette>
#include <QVariantMap>
#include <catch2/catch_test_macros.hpp>

namespace {

void verifyConsistency(bool dark, bool has_wallpaper)
{
    const QPalette palette = app::core::buildThemePalette(dark, has_wallpaper);

    dock::ui::DockTheme dock_theme;
    dock_theme.setDark(dark);
    dock_theme.setWallpaperUrl(has_wallpaper ? QStringLiteral("image://wallpaper/test.png")
                                             : QString());
    const QVariantMap colors = dock_theme.colors();

    auto dock_color = [&colors](const char* key) {
        return colors.value(key).value<QColor>();
    };

    REQUIRE(palette.color(QPalette::Window) == dock_color("windowBackground"));
    REQUIRE(palette.color(QPalette::Base) == dock_color("surface"));
    REQUIRE(palette.color(QPalette::AlternateBase) == dock_color("surfaceAlt"));
    REQUIRE(palette.color(QPalette::Text) == dock_color("textPrimary"));
    REQUIRE(palette.color(QPalette::WindowText) == dock_color("textPrimary"));
    REQUIRE(palette.color(QPalette::Button) == dock_color("surfaceAlt"));
    REQUIRE(palette.color(QPalette::ButtonText) == dock_color("textPrimary"));
    REQUIRE(palette.color(QPalette::Highlight) == dock_color("primary"));
    REQUIRE(palette.color(QPalette::HighlightedText) == dock_color("textOnPrimary"));
    REQUIRE(palette.color(QPalette::PlaceholderText) == dock_color("textDisabled"));
    REQUIRE(palette.color(QPalette::Mid) == dock_color("borderStrong"));
    REQUIRE(palette.color(QPalette::Midlight) == dock_color("border"));
    REQUIRE(palette.color(QPalette::Light) == dock_color("surface"));
    REQUIRE(palette.color(QPalette::Dark) == dock_color("borderStrong"));
    REQUIRE(palette.color(QPalette::Shadow) == dock_color("scrollBarHover"));
    REQUIRE(palette.color(QPalette::Link) == dock_color("primary"));
    // 弹窗面为不透明 menuSurface（ToolTip 等按应用调色板取色）
    REQUIRE(palette.color(QPalette::ToolTipBase) == dock_color("menuSurface"));
    REQUIRE(palette.color(QPalette::ToolTipText) == dock_color("textPrimary"));
}

}

TEST_CASE("Theme consistency: light, no wallpaper", "[ThemeConsistency]")
{
    verifyConsistency(false, false);
}

TEST_CASE("Theme consistency: light, wallpaper", "[ThemeConsistency]")
{
    verifyConsistency(false, true);
}

TEST_CASE("Theme consistency: dark, no wallpaper", "[ThemeConsistency]")
{
    verifyConsistency(true, false);
}

TEST_CASE("Theme consistency: dark, wallpaper", "[ThemeConsistency]")
{
    verifyConsistency(true, true);
}
