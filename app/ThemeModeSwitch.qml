/**
 * @file ThemeModeSwitch.qml
 * @brief 明暗主题两段开关：选中段带滑动指示，仅依赖 Theme 令牌
 */

import QtQuick
import QtQuick.Controls

Item {
    id: root

    //! @brief 当前选中的段（0 = 浅色，1 = 深色）
    property int currentIndex: 0
    //! @brief 用户点击某一段时发出，参数为段下标
    signal activated(int index)

    implicitWidth: 176
    implicitHeight: 30

    readonly property real segmentWidth: (width - 4) / 2

    Rectangle {
        id: track
        anchors.fill: parent
        radius: Theme.radiusMenu
        color: Theme.surfaceAlt
        border.width: 1
        border.color: Theme.border
    }

    Rectangle {
        id: indicator
        x: 2 + root.currentIndex * root.segmentWidth
        y: 2
        width: root.segmentWidth
        height: parent.height - 4
        radius: Theme.radiusControl
        color: Theme.menuSurface
        border.width: 1
        border.color: Theme.borderStrong

        Behavior on x {
            NumberAnimation {
                duration: 160
                easing.type: Easing.OutCubic
            }
        }
    }

    Row {
        anchors.fill: parent
        anchors.margins: 2
        spacing: 0

        Repeater {
            model: [qsTr("浅色"), qsTr("深色")]

            Button {
                id: segment

                width: root.segmentWidth
                height: parent.height
                padding: 0
                hoverEnabled: true

                Accessible.role: Accessible.RadioButton
                Accessible.name: modelData
                Accessible.checkable: true
                Accessible.checked: root.currentIndex === index

                background: Rectangle {
                    radius: Theme.radiusControl
                    color: "transparent"
                    border.width: segment.visualFocus ? 1 : 0
                    border.color: Theme.primary
                }

                contentItem: Text {
                    text: modelData
                    color: root.currentIndex === index ? Theme.primary : segment.hovered ? Theme.textPrimary : Theme.textSecondary
                    font.pixelSize: Theme.fontSizeBody
                    font.weight: root.currentIndex === index ? Font.DemiBold : Font.Normal
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: root.activated(index)
            }
        }
    }
}
