/**
 * @file ThemedDialog.qml
 * @brief 跟随主题令牌的对话框：显式绑定 palette，避免独立弹窗按创建快照取色
 *
 * 菜单/对话框的 palette 不会随窗口调色板的后续变化自动更新；统一在此绑定
 * Theme 令牌。弹窗面使用不透明 menuSurface，保证壁纸背景上的文字可读。
 */

import QtQuick
import QtQuick.Controls

Dialog {
    palette.window: Theme.menuSurface
    palette.base: Theme.menuSurface
    palette.windowText: Theme.textPrimary
    palette.text: Theme.textPrimary
    palette.button: Theme.menuSurface
    palette.buttonText: Theme.textPrimary
    palette.highlight: Theme.primary
    palette.highlightedText: Theme.textOnPrimary
    palette.mid: Theme.borderStrong
    palette.midlight: Theme.border
    palette.dark: Theme.borderStrong
    palette.light: Theme.menuSurface
    palette.shadow: Theme.scrollBarHover
    palette.placeholderText: Theme.textDisabled
}
