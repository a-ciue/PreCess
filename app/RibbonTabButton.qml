/**
 * @file RibbonTabButton.qml
 * @brief ribbon 分类页签按钮：选中态为强调色文字 + 底部下划线
 */

import QtQuick
import QtQuick.Controls.Fusion

ToolButton {
    id: control

    background: Rectangle {
        radius: Theme.radiusControl
        // 自定义背景需显式保留按下、禁用和键盘焦点反馈。
        color: !control.enabled ? "transparent"
             : control.down ? Theme.primaryTint
             : control.checked ? Theme.primaryTint
             : control.hovered ? Theme.hoverOverlay : "transparent"
        border.width: control.visualFocus ? 1 : 0
        border.color: Theme.primary

        // 选中页签的底部强调条
        Rectangle {
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            height: 2
            radius: 1
            color: control.enabled ? Theme.primary : Theme.textDisabled
            visible: control.checked
        }
    }

    contentItem: Text {
        text: control.text
        font.pixelSize: Theme.fontSizeBody
        font.weight: control.checked ? Font.DemiBold : Font.Normal
        color: !control.enabled ? Theme.textDisabled
             : control.down ? Theme.primaryPressed
             : control.checked ? Theme.primary : control.hovered ? Theme.primaryHover : Theme.textPrimary
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
