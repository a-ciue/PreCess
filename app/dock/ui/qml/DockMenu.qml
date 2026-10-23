/**
 * @file DockMenu.qml
 * @brief 停靠组件菜单：显式绑定 DockTheme 弹窗调色板，使菜单随主题即时切换
 *
 * dock 模块刻意仅依赖 Qt、不反向依赖 app；配色由 app 侧推送为已解析的
 * Docking.Theme.popupPalette。菜单弹窗的 palette 不会随窗口调色板后续变化
 * 自动更新，故显式绑定。新菜单请使用本类型而非裸 Menu。
 */

import QtQuick
import QtQuick.Controls
import PreCess.Docking as Docking

Menu {
    palette.window: Docking.Theme.popupPalette.window
    palette.base: Docking.Theme.popupPalette.base
    palette.alternateBase: Docking.Theme.popupPalette.alternateBase
    palette.windowText: Docking.Theme.popupPalette.windowText
    palette.text: Docking.Theme.popupPalette.text
    palette.button: Docking.Theme.popupPalette.button
    palette.buttonText: Docking.Theme.popupPalette.buttonText
    palette.highlight: Docking.Theme.popupPalette.highlight
    palette.highlightedText: Docking.Theme.popupPalette.highlightedText
    palette.placeholderText: Docking.Theme.popupPalette.placeholderText
    palette.mid: Docking.Theme.popupPalette.mid
    palette.midlight: Docking.Theme.popupPalette.midlight
    palette.light: Docking.Theme.popupPalette.light
    palette.dark: Docking.Theme.popupPalette.dark
    palette.shadow: Docking.Theme.popupPalette.shadow
    palette.link: Docking.Theme.popupPalette.link
    palette.toolTipBase: Docking.Theme.popupPalette.toolTipBase
    palette.toolTipText: Docking.Theme.popupPalette.toolTipText
}
