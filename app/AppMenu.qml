/**
 * @file AppMenu.qml
 * @brief 应用菜单：显式绑定主题调色板，使菜单弹窗随主题即时切换
 *
 * 菜单弹窗的 palette 不会随窗口调色板的后续变化自动更新（独立弹窗按创建时
 * 快照），故此处按 Theme 角色令牌显式绑定——与 Qt 自带 Fusion ComboBox 的
 * `palette: control.palette` 同一手法。新菜单请使用本类型而非裸 Menu。
 */

import QtQuick
import QtQuick.Controls

Menu {
    palette.window: Theme.surface
    palette.base: Theme.surface
    palette.alternateBase: Theme.surfaceAlt
    palette.windowText: Theme.textPrimary
    palette.text: Theme.textPrimary
    palette.button: Theme.surfaceAlt
    palette.buttonText: Theme.textPrimary
    palette.highlight: Theme.primary
    palette.highlightedText: Theme.textOnPrimary
    palette.placeholderText: Theme.textDisabled
    palette.mid: Theme.borderStrong
    palette.midlight: Theme.border
    palette.light: Theme.surface
    palette.dark: Theme.borderStrong
    palette.shadow: Theme.scrollBarHover
    palette.link: Theme.primary
}
