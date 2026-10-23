/**
 * @file ThemePalette.h
 * @brief 主题应用调色板构造：把 Theme 已解析的角色颜色表转成 QPalette
 *
 * 颜色值与角色对应关系均由 app/Theme.qml 决定；本文件只做 map -> QPalette 的
 * 机械适配，不包含明暗、壁纸或任何产品色常量，便于单测。
 */

#pragma once

#include <QColor>
#include <QPalette>
#include <QVariant>
#include <QVariantMap>

namespace app::core {

/**
 * @brief 读取角色颜色；缺失或无效时跳过，保留 QPalette 原值
 * @param palette 目标调色板
 * @param role 目标角色
 * @param colors Theme 推送的角色表
 * @param key 角色键（与 QML palette.* 同名）
 */
inline void setThemeColor(QPalette& palette, QPalette::ColorRole role,
    const QVariantMap& colors, const char* key)
{
    const QVariant value = colors.value(QLatin1String(key));
    if (!value.isValid())
        return;

    const QColor color = value.canConvert<QColor>() ? value.value<QColor>()
                                                    : QColor(value.toString());
    if (color.isValid())
        palette.setColor(role, color);
}

/** @brief 将 Theme.controlPalette 的已解析角色颜色组装为应用调色板 */
inline QPalette buildThemePalette(const QVariantMap& colors)
{
    QPalette palette;
    setThemeColor(palette, QPalette::Window, colors, "window");
    setThemeColor(palette, QPalette::WindowText, colors, "windowText");
    setThemeColor(palette, QPalette::Base, colors, "base");
    setThemeColor(palette, QPalette::AlternateBase, colors, "alternateBase");
    setThemeColor(palette, QPalette::Text, colors, "text");
    setThemeColor(palette, QPalette::Button, colors, "button");
    setThemeColor(palette, QPalette::ButtonText, colors, "buttonText");
    setThemeColor(palette, QPalette::Highlight, colors, "highlight");
    setThemeColor(palette, QPalette::HighlightedText, colors, "highlightedText");
    setThemeColor(palette, QPalette::PlaceholderText, colors, "placeholderText");
    setThemeColor(palette, QPalette::Mid, colors, "mid");
    setThemeColor(palette, QPalette::Midlight, colors, "midlight");
    setThemeColor(palette, QPalette::Light, colors, "light");
    setThemeColor(palette, QPalette::Dark, colors, "dark");
    setThemeColor(palette, QPalette::Shadow, colors, "shadow");
    setThemeColor(palette, QPalette::Link, colors, "link");
    setThemeColor(palette, QPalette::ToolTipBase, colors, "toolTipBase");
    setThemeColor(palette, QPalette::ToolTipText, colors, "toolTipText");
    return palette;
}

}
