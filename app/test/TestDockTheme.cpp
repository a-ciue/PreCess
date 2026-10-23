/**
 * @file TestDockTheme.cpp
 * @brief DockTheme 单元测试：宿主推送的配色表与壁纸源按原值保存并通知
 */
#include "ui/DockTheme.h"

#include <QColor>
#include <QString>
#include <QVariantMap>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("DockTheme: stores pushed maps and notifies on change", "[DockTheme]")
{
    dock::ui::DockTheme theme;
    int colors_changes = 0;
    int control_changes = 0;
    int popup_changes = 0;
    QObject::connect(&theme, &dock::ui::DockTheme::colorsChanged, [&] { ++colors_changes; });
    QObject::connect(&theme, &dock::ui::DockTheme::controlPaletteChanged, [&] { ++control_changes; });
    QObject::connect(&theme, &dock::ui::DockTheme::popupPaletteChanged, [&] { ++popup_changes; });

    const QVariantMap colors {
        { QStringLiteral("surface"), QColor("#112233") },
        { QStringLiteral("wallpaperTint"), QColor("#445566") }
    };
    const QVariantMap control {
        { QStringLiteral("base"), QColor("#778899") },
        { QStringLiteral("text"), QColor("#aabbcc") }
    };
    const QVariantMap popup {
        { QStringLiteral("window"), QColor("#ddeeff") }
    };

    theme.setColors(colors);
    theme.setControlPalette(control);
    theme.setPopupPalette(popup);

    REQUIRE(theme.colors() == colors);
    REQUIRE(theme.controlPalette() == control);
    REQUIRE(theme.popupPalette() == popup);
    REQUIRE(colors_changes == 1);
    REQUIRE(control_changes == 1);
    REQUIRE(popup_changes == 1);

    // 相同值不重复通知
    theme.setColors(colors);
    theme.setControlPalette(control);
    theme.setPopupPalette(popup);
    REQUIRE(colors_changes == 1);
    REQUIRE(control_changes == 1);
    REQUIRE(popup_changes == 1);
}

TEST_CASE("DockTheme: wallpaper url propagates", "[DockTheme]")
{
    dock::ui::DockTheme theme;
    REQUIRE(theme.wallpaperUrl().isEmpty());

    int changes = 0;
    QObject::connect(&theme, &dock::ui::DockTheme::wallpaperUrlChanged, [&] { ++changes; });

    theme.setWallpaperUrl(QStringLiteral("image://wallpaper/abc.png"));
    REQUIRE(theme.wallpaperUrl() == QStringLiteral("image://wallpaper/abc.png"));
    REQUIRE(changes == 1);

    theme.setWallpaperUrl(QStringLiteral("image://wallpaper/abc.png"));
    REQUIRE(changes == 1);
}
