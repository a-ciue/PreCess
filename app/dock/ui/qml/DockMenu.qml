/**
 * @file DockMenu.qml
 * @brief 停靠组件菜单：显式绑定 DockTheme 配色，使菜单弹窗随主题即时切换
 *
 * dock 模块刻意仅依赖 Qt、不反向依赖 app，故取自 Docking.Theme.colors
 * （与 app/Theme.qml 对应令牌同步）。菜单弹窗的 palette 不会随窗口调色板后续
 * 变化自动更新，故显式绑定。新菜单请使用本类型而非裸 Menu。
 */

import QtQuick
import QtQuick.Controls
import PreCess.Docking as Docking

Menu {
    palette.window: Docking.Theme.colors.surface
    palette.base: Docking.Theme.colors.surface
    palette.alternateBase: Docking.Theme.colors.surfaceAlt
    palette.windowText: Docking.Theme.colors.textPrimary
    palette.text: Docking.Theme.colors.textPrimary
    palette.button: Docking.Theme.colors.surfaceAlt
    palette.buttonText: Docking.Theme.colors.textPrimary
    palette.highlight: Docking.Theme.colors.primary
    palette.highlightedText: Docking.Theme.colors.textOnPrimary
    palette.placeholderText: Docking.Theme.colors.textDisabled
    palette.mid: Docking.Theme.colors.borderStrong
    palette.midlight: Docking.Theme.colors.border
    palette.light: Docking.Theme.colors.surface
    palette.dark: Docking.Theme.colors.borderStrong
    palette.shadow: Docking.Theme.colors.scrollBarHover
    palette.link: Docking.Theme.colors.primary
}
