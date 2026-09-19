/**
 * @file Divider.qml
 * @brief 分隔条视图：拖动调整相邻面板尺寸
 */

import QtQuick

Rectangle {
    id: root

    //! @brief 关联的 DividerItem（C++）
    property var dividerItem: null

    color: hoverHandler.hovered ? "#7aa7d9" : "transparent"

    HoverHandler {
        id: hoverHandler
        cursorShape: root.dividerItem && root.dividerItem.horizontal ? Qt.SizeHorCursor : Qt.SizeVerCursor
    }

    MouseArea {
        anchors {
            fill: parent
            leftMargin: root.dividerItem && root.dividerItem.horizontal ? -2 : 0
            rightMargin: root.dividerItem && root.dividerItem.horizontal ? -2 : 0
            topMargin: root.dividerItem && !root.dividerItem.horizontal ? -2 : 0
            bottomMargin: root.dividerItem && !root.dividerItem.horizontal ? -2 : 0
        }
        acceptedButtons: Qt.LeftButton
        onPressed: root.dividerItem.beginGroupDrag(root.mapToGlobal(mouse.x, mouse.y))
        onPositionChanged: function(mouse) {
            if (pressed)
                root.dividerItem.dragTo(root.mapToGlobal(mouse.x, mouse.y))
        }
        onReleased: root.dividerItem.endDrag()
    }
}
