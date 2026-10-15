/**
 * @file ThemePalette.h
 * @brief 主题应用调色板构造：纯函数，不依赖 QML 与应用实例，便于单测
 *
 * 角色色值须与 app/Theme.qml 保持一致；AppPalette 单例据此刷新应用调色板。
 */

#pragma once

#include <QColor>
#include <QPalette>

namespace app::core {

/** @brief 组装指定明暗的应用调色板 */
inline QPalette buildThemePalette(bool dark)
{
    auto themed = [dark](const char* light, const char* dark_hex) {
        return QColor(QLatin1String(dark ? dark_hex : light));
    };
    // 半透明面令牌（与 Theme.qml 的 surface / surfaceAlt 同步）；深浅主题均为半透明
    // 以便透出壁纸背景。
    auto themed_alpha = [dark](const char* light, const char* dark_hex, qreal alpha) {
        QColor color(QLatin1String(dark ? dark_hex : light));
        color.setAlphaF(alpha);
        return color;
    };

    QPalette palette;
    palette.setColor(QPalette::Window, themed("#F3F4F6", "#1E1E1E"));
    palette.setColor(QPalette::WindowText, themed("#2B2F36", "#D4D4D4"));
    palette.setColor(QPalette::Base, themed_alpha("#FFFFFF", "#252526", 0.72));
    palette.setColor(QPalette::AlternateBase, themed_alpha("#F7F8FA", "#2D2D30", 0.62));
    palette.setColor(QPalette::Text, themed("#2B2F36", "#D4D4D4"));
    palette.setColor(QPalette::Button, themed_alpha("#F7F8FA", "#2D2D30", 0.62));
    palette.setColor(QPalette::ButtonText, themed("#2B2F36", "#D4D4D4"));
    palette.setColor(QPalette::Highlight, themed("#1976D2", "#4EA1F3"));
    palette.setColor(QPalette::HighlightedText, themed("#FFFFFF", "#FFFFFF"));
    palette.setColor(QPalette::PlaceholderText, themed("#9CA3AF", "#6B6F76"));
    palette.setColor(QPalette::Mid, themed("#C3C8CF", "#4A4A4F"));
    palette.setColor(QPalette::Midlight, themed("#D9DDE3", "#3E3E42"));
    palette.setColor(QPalette::Light, themed_alpha("#FFFFFF", "#252526", 0.72));
    palette.setColor(QPalette::Dark, themed("#C3C8CF", "#4A4A4F"));
    palette.setColor(QPalette::Shadow, themed("#B7BDC7", "#5A5A5A"));
    palette.setColor(QPalette::Link, themed("#1976D2", "#4EA1F3"));
    palette.setColor(QPalette::ToolTipBase, themed("#FFFFFF", "#252526"));
    palette.setColor(QPalette::ToolTipText, themed("#2B2F36", "#D4D4D4"));
    return palette;
}

}
