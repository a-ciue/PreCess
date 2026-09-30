/**
 * @file RibbonActionButton.qml
 * @brief ribbon 页内动作按钮：图标在上文字在下，悬停/选中带浅色着色
 */

import QtQuick
import QtQuick.Controls

ToolButton {
    id: control

    display: ToolButton.TextUnderIcon
    leftPadding: 10
    rightPadding: 10

    background: Rectangle {
        radius: Theme.radiusControl
        color: control.checked ? Theme.primaryTint : control.hovered ? Theme.hoverOverlay : "transparent"
    }
}
