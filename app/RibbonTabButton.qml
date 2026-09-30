/**
 * @file RibbonTabButton.qml
 * @brief ribbon 分类页签按钮：选中态为强调色文字 + 底部下划线
 */

import QtQuick
import QtQuick.Controls

ToolButton {
    id: control

    background: Rectangle {
        radius: Theme.radiusControl
        color: control.checked ? Theme.primaryTint : control.hovered ? Theme.hoverOverlay : "transparent"

        // 选中页签的底部强调条
        Rectangle {
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            height: 2
            radius: 1
            color: Theme.primary
            visible: control.checked
        }
    }

    contentItem: Text {
        text: control.text
        font.pixelSize: Theme.fontSizeBody
        font.weight: control.checked ? Font.DemiBold : Font.Normal
        color: control.checked ? Theme.primary : control.hovered ? Theme.primaryHover : Theme.textPrimary
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
