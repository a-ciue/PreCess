/**
 * @file DockMenu.qml
 * @brief 停靠组件菜单：显式绑定明暗调色板，使菜单弹窗随主题即时切换
 *
 * dock 模块刻意仅依赖 Qt、不反向依赖 app，故以 Docking.Theme.dark 选择本地
 * 色对；色值须与 app/Theme.qml 对应令牌保持同步。菜单弹窗的 palette 不会随
 * 窗口调色板后续变化自动更新，故显式绑定。新菜单请使用本类型而非裸 Menu。
 */

import QtQuick
import QtQuick.Controls
import PreCess.Docking as Docking

Menu {
    readonly property bool dark: Docking.Theme.dark
    palette.window: dark ? "#252526" : "#FFFFFF"
    palette.base: dark ? "#252526" : "#FFFFFF"
    palette.alternateBase: dark ? "#2D2D30" : "#F7F8FA"
    palette.windowText: dark ? "#D4D4D4" : "#2B2F36"
    palette.text: dark ? "#D4D4D4" : "#2B2F36"
    palette.button: dark ? "#2D2D30" : "#F7F8FA"
    palette.buttonText: dark ? "#D4D4D4" : "#2B2F36"
    palette.highlight: dark ? "#4EA1F3" : "#1976D2"
    palette.highlightedText: "#FFFFFF"
    palette.placeholderText: dark ? "#6B6F76" : "#9CA3AF"
    palette.mid: dark ? "#4A4A4F" : "#C3C8CF"
    palette.midlight: dark ? "#3E3E42" : "#D9DDE3"
    palette.light: dark ? "#252526" : "#FFFFFF"
    palette.dark: dark ? "#4A4A4F" : "#C3C8CF"
    palette.shadow: dark ? "#5A5A5A" : "#B7BDC7"
    palette.link: dark ? "#4EA1F3" : "#1976D2"
}
