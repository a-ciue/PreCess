/**
 * @file DockSeparator.qml
 * @brief 分隔条视图：拖动调整相邻面板尺寸
 */

import QtQuick

Rectangle {
    id: root

    //! @brief 关联的 SeparatorView（C++）
    property var separatorView: null

    color: hoverHandler.hovered ? "#7aa7d9" : "transparent"

    HoverHandler {
        id: hoverHandler
        cursorShape: root.separatorView && root.separatorView.horizontal ? Qt.SizeHorCursor : Qt.SizeVerCursor
    }

    MouseArea {
        anchors {
            fill: parent
            leftMargin: root.separatorView && root.separatorView.horizontal ? -2 : 0
            rightMargin: root.separatorView && root.separatorView.horizontal ? -2 : 0
            topMargin: root.separatorView && !root.separatorView.horizontal ? -2 : 0
            bottomMargin: root.separatorView && !root.separatorView.horizontal ? -2 : 0
        }
        acceptedButtons: Qt.LeftButton
        onPressed: root.separatorView.beginDrag(root.mapToGlobal(mouse.x, mouse.y))
        onPositionChanged: function(mouse) {
            if (pressed)
                root.separatorView.dragTo(root.mapToGlobal(mouse.x, mouse.y))
        }
        onReleased: root.separatorView.endDrag()
    }
}
