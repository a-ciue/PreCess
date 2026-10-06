/**
 * @file RibbonActionButton.qml
 * @brief ribbon 页内动作按钮：图标在上文字在下，悬停/选中带浅色着色
 */

import QtQuick
import QtQuick.Controls.Fusion

ToolButton {
    id: control

    display: ToolButton.TextUnderIcon
    leftPadding: 10
    rightPadding: 10

    background: Rectangle {
        radius: Theme.radiusControl
        // 自定义背景需显式保留按下、禁用和键盘焦点反馈。
        color: !control.enabled ? "transparent"
             : control.down ? Theme.primaryTint
             : control.checked ? Theme.primaryTint
             : control.hovered ? Theme.hoverOverlay : "transparent"
        border.width: control.visualFocus ? 1 : 0
        border.color: Theme.primary
    }
}
