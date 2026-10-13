/**
 * @file TestDockTheme.cpp
 * @brief DockTheme 单元测试：明暗切换后配色表整体刷新
 */
#include "ui/DockTheme.h"

#include <catch2/catch_test_macros.hpp>
#include <QColor>
#include <QVariantMap>

TEST_CASE("DockTheme: default light colors", "[DockTheme]")
{
    dock::ui::DockTheme theme;
    REQUIRE_FALSE(theme.dark());

    const QVariantMap light = theme.colors();
    REQUIRE(light.value("border").value<QColor>() == QColor("#D9DDE3"));
    REQUIRE(light.value("surface").value<QColor>() == QColor("#FFFFFF"));
    REQUIRE(light.value("panelHeaderSurface").value<QColor>() == QColor("#E6EDF5"));
    REQUIRE(light.value("dropAccent").value<QColor>() == QColor("#2f6fb5"));
}

TEST_CASE("DockTheme: dark colors after setDark", "[DockTheme]")
{
    dock::ui::DockTheme theme;
    theme.setDark(true);
    REQUIRE(theme.dark());

    const QVariantMap dark = theme.colors();
    REQUIRE(dark.value("border").value<QColor>() == QColor("#3E3E42"));
    REQUIRE(dark.value("surface").value<QColor>() == QColor("#252526"));
    REQUIRE(dark.value("textPrimary").value<QColor>() == QColor("#D4D4D4"));
    REQUIRE(dark.value("primary").value<QColor>() == QColor("#4EA1F3"));
    REQUIRE(dark.value("dropAccent").value<QColor>() == QColor("#4EA1F3"));
}
