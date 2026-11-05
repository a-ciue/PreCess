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
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

namespace app::core {

//! @brief 主题角色与 QML 角色键的对应关系；AppPalette 与 Theme.qml 的契约
struct ThemeRoleBinding {
    QPalette::ColorRole role;
    const char* key;
};

inline constexpr ThemeRoleBinding kThemeRoles[] {
    { QPalette::Window, "window" },
    { QPalette::WindowText, "windowText" },
    { QPalette::Base, "base" },
    { QPalette::AlternateBase, "alternateBase" },
    { QPalette::Text, "text" },
    { QPalette::Button, "button" },
    { QPalette::ButtonText, "buttonText" },
    { QPalette::Highlight, "highlight" },
    { QPalette::HighlightedText, "highlightedText" },
    { QPalette::PlaceholderText, "placeholderText" },
    { QPalette::Mid, "mid" },
    { QPalette::Midlight, "midlight" },
    { QPalette::Light, "light" },
    { QPalette::Dark, "dark" },
    { QPalette::Shadow, "shadow" },
    { QPalette::Link, "link" },
    { QPalette::ToolTipBase, "toolTipBase" },
    { QPalette::ToolTipText, "toolTipText" },
};

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

/** @brief 返回 roles 中缺失于 colors 的角色键，供契约测试与诊断使用 */
inline QStringList missingThemeRoles(const QVariantMap& colors)
{
    QStringList missing;
    for (const ThemeRoleBinding& binding : kThemeRoles) {
        if (!colors.contains(QLatin1String(binding.key)))
            missing.append(QLatin1String(binding.key));
    }
    return missing;
}

/** @brief 将 Theme.controlPalette 的已解析角色颜色组装为应用调色板 */
inline QPalette buildThemePalette(const QVariantMap& colors)
{
    QPalette palette;
    for (const ThemeRoleBinding& binding : kThemeRoles)
        setThemeColor(palette, binding.role, colors, binding.key);
    return palette;
}

}
