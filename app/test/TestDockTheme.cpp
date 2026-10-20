/**
 * @file TestDockTheme.cpp
 * @brief DockTheme 单元测试：明暗切换后配色表整体刷新
 */
#include "ui/DockTheme.h"

#include <QColor>
#include <QVariantMap>
#include <catch2/catch_test_macros.hpp>

namespace {

//! @brief 构造带 alpha 的期望色（与 DockTheme 的半透明面令牌一致）
QColor withAlpha(const char* hex, qreal alpha)
{
    QColor color { QLatin1String(hex) };
    color.setAlphaF(alpha);
    return color;
}

}

TEST_CASE("DockTheme: default light colors", "[DockTheme]")
{
    dock::ui::DockTheme theme;
    REQUIRE_FALSE(theme.dark());

    // 无壁纸：面令牌保持不透明
    const QVariantMap light = theme.colors();
    REQUIRE(light.value("border").value<QColor>() == QColor("#D9DDE3"));
    REQUIRE(light.value("surface").value<QColor>() == QColor("#FFFFFF"));
    REQUIRE(light.value("surfaceAlt").value<QColor>() == QColor("#F7F8FA"));
    REQUIRE(light.value("panelHeaderSurface").value<QColor>() == QColor("#E6EDF5"));
    // 菜单面保持不透明
    REQUIRE(light.value("menuSurface").value<QColor>() == QColor("#FFFFFF"));
    // 壁纸遮罩：浅色白亮化
    REQUIRE(light.value("wallpaperTint").value<QColor>() == withAlpha("#FFFFFF", 0.45));
    REQUIRE(light.value("dropAccent").value<QColor>() == QColor("#2f6fb5"));
    REQUIRE(light.value("dropZoneActiveFore").value<QColor>() == QColor("#FFFFFF"));

    // 有壁纸：面令牌切换为半透明
    theme.setWallpaperUrl(QStringLiteral("image://wallpaper/abc.png"));
    const QVariantMap wallpaper = theme.colors();
    REQUIRE(wallpaper.value("surface").value<QColor>() == withAlpha("#FFFFFF", 0.72));
    REQUIRE(wallpaper.value("surfaceAlt").value<QColor>() == withAlpha("#F7F8FA", 0.62));
    REQUIRE(wallpaper.value("panelHeaderSurface").value<QColor>() == withAlpha("#E6EDF5", 0.70));
}

TEST_CASE("DockTheme: dark colors after setDark", "[DockTheme]")
{
    dock::ui::DockTheme theme;
    theme.setWallpaperUrl(QStringLiteral("image://wallpaper/abc.png"));
    theme.setDark(true);
    REQUIRE(theme.dark());

    const QVariantMap dark = theme.colors();
    REQUIRE(dark.value("border").value<QColor>() == QColor("#3E3E42"));
    REQUIRE(dark.value("surface").value<QColor>() == withAlpha("#252526", 0.72));
    REQUIRE(dark.value("textPrimary").value<QColor>() == QColor("#D4D4D4"));
    REQUIRE(dark.value("primary").value<QColor>() == QColor("#4EA1F3"));
    REQUIRE(dark.value("menuSurface").value<QColor>() == QColor("#252526"));
    // 壁纸遮罩：深色灰暗化
    REQUIRE(dark.value("wallpaperTint").value<QColor>() == withAlpha("#1F1F24", 0.55));
    REQUIRE(dark.value("dropAccent").value<QColor>() == QColor("#4EA1F3"));
    REQUIRE(dark.value("dropZoneActiveFore").value<QColor>() == QColor("#FFFFFF"));
}

TEST_CASE("DockTheme: clearing wallpaper restores opaque surfaces", "[DockTheme]")
{
    dock::ui::DockTheme theme;
    theme.setWallpaperUrl(QStringLiteral("image://wallpaper/abc.png"));
    REQUIRE(theme.colors().value("surface").value<QColor>() == withAlpha("#FFFFFF", 0.72));

    theme.setWallpaperUrl(QString());
    REQUIRE(theme.wallpaperUrl().isEmpty());
    REQUIRE(theme.colors().value("surface").value<QColor>() == QColor("#FFFFFF"));
    REQUIRE(theme.colors().value("surfaceAlt").value<QColor>() == QColor("#F7F8FA"));
}

TEST_CASE("DockTheme: wallpaper url propagates", "[DockTheme]")
{
    dock::ui::DockTheme theme;
    REQUIRE(theme.wallpaperUrl().isEmpty());
    theme.setWallpaperUrl(QStringLiteral("image://wallpaper/abc.png"));
    REQUIRE(theme.wallpaperUrl() == QStringLiteral("image://wallpaper/abc.png"));
}
