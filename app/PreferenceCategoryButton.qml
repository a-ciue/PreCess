/**
 * @file PreferenceCategoryButton.qml
 * @brief 偏好设置分类按钮：纵向分类栏与窄面板横向标签共用
 *
 * 纵向模式用浅色底表达当前分类；横向模式用底部强调条，与 ribbon 页签一致。
 */

import QtQuick
import QtQuick.Controls

ToolButton {
    id: control

    //! @brief 当前是否为选中分类（由页面栈下标驱动）
    property bool active: false
    //! @brief 横向标签模式；false 时为纵向分类栏模式
    property bool tabMode: false

    implicitHeight: tabMode ? 38 : 34
    leftPadding: tabMode ? Theme.spacingSm : Theme.spacingMd
    rightPadding: tabMode ? Theme.spacingSm : Theme.spacingMd
    padding: 0
    hoverEnabled: true

    Accessible.role: Accessible.PageTab
    Accessible.name: text
    Accessible.checkable: true
    Accessible.checked: active

    background: Rectangle {
        radius: Theme.radiusControl
        color: control.tabMode ? (control.hovered ? Theme.hoverOverlay : "transparent") : control.active ? Theme.primaryTint : control.hovered ? Theme.hoverOverlay : "transparent"
        border.width: control.visualFocus ? 1 : 0
        border.color: Theme.primary

        // 横向标签的选中强调条，复用 ribbon 页签的视觉语言
        Rectangle {
            visible: control.tabMode && control.active
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            height: 2
            radius: 1
            color: Theme.primary
        }
    }

    contentItem: Text {
        text: control.text
        color: control.active ? Theme.primary : control.hovered ? Theme.primaryHover : Theme.textPrimary
        font.pixelSize: Theme.fontSizeBody
        font.weight: control.active ? Font.DemiBold : Font.Normal
        horizontalAlignment: control.tabMode ? Text.AlignHCenter : Text.AlignLeft
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
