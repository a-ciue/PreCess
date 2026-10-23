/**
 * @file AppMenu.qml
 * @brief 应用菜单：显式绑定主题调色板，使菜单弹窗随主题即时切换
 *
 * 菜单弹窗的 palette 不会随窗口调色板的后续变化自动更新（独立弹窗按创建时
 * 快照），故此处绑定 Theme.popupPalette——颜色值与角色对应关系只在 Theme.qml
 * 维护，本文件只消费结果。新菜单请使用本类型而非裸 Menu。
 */

import QtQuick
import QtQuick.Controls

Menu {
    palette.window: Theme.popupPalette.window
    palette.base: Theme.popupPalette.base
    palette.alternateBase: Theme.popupPalette.alternateBase
    palette.windowText: Theme.popupPalette.windowText
    palette.text: Theme.popupPalette.text
    palette.button: Theme.popupPalette.button
    palette.buttonText: Theme.popupPalette.buttonText
    palette.highlight: Theme.popupPalette.highlight
    palette.highlightedText: Theme.popupPalette.highlightedText
    palette.placeholderText: Theme.popupPalette.placeholderText
    palette.mid: Theme.popupPalette.mid
    palette.midlight: Theme.popupPalette.midlight
    palette.light: Theme.popupPalette.light
    palette.dark: Theme.popupPalette.dark
    palette.shadow: Theme.popupPalette.shadow
    palette.link: Theme.popupPalette.link
    palette.toolTipBase: Theme.popupPalette.toolTipBase
    palette.toolTipText: Theme.popupPalette.toolTipText
}
