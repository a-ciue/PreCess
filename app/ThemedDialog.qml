/**
 * @file ThemedDialog.qml
 * @brief 跟随主题令牌的对话框：显式绑定 palette，避免独立弹窗按创建快照取色
 *
 * 菜单/对话框的 palette 不会随窗口调色板的后续变化自动更新；统一在此消费
 * Theme.popupPalette。弹窗面保持不透明，保证壁纸背景上的文字可读。
 */

import QtQuick
import QtQuick.Controls

Dialog {
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
