/**
 * @file TestAppPalette.cpp
 * @brief 主题应用调色板单元测试：角色随明暗正确解析
 */
#include "core/ThemePalette.h"

#include <catch2/catch_test_macros.hpp>
#include <QColor>
#include <QPalette>

TEST_CASE("ThemePalette: light roles", "[ThemePalette]")
{
    const QPalette p = app::core::buildThemePalette(false);
    REQUIRE(p.color(QPalette::Window) == QColor("#F3F4F6"));
    REQUIRE(p.color(QPalette::Base) == QColor("#FFFFFF"));
    REQUIRE(p.color(QPalette::Text) == QColor("#2B2F36"));
    REQUIRE(p.color(QPalette::Highlight) == QColor("#1976D2"));
    REQUIRE(p.color(QPalette::ToolTipBase) == QColor("#FFFFFF"));
    REQUIRE(p.color(QPalette::ToolTipText) == QColor("#2B2F36"));
}

TEST_CASE("ThemePalette: dark roles", "[ThemePalette]")
{
    const QPalette p = app::core::buildThemePalette(true);
    REQUIRE(p.color(QPalette::Window) == QColor("#1E1E1E"));
    REQUIRE(p.color(QPalette::Base) == QColor("#252526"));
    REQUIRE(p.color(QPalette::Text) == QColor("#D4D4D4"));
    REQUIRE(p.color(QPalette::Highlight) == QColor("#4EA1F3"));
    REQUIRE(p.color(QPalette::ToolTipBase) == QColor("#252526"));
    REQUIRE(p.color(QPalette::ToolTipText) == QColor("#D4D4D4"));
}
