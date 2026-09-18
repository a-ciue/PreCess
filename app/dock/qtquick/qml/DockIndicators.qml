/**
 * @file DockIndicators.qml
 * @brief 拖放落点高亮（在透明顶层指示器窗口中绘制）
 */

import QtQuick

Item {
    id: root

    //! @brief 是否显示高亮
    property bool active: false
    //! @brief 高亮矩形（本窗口坐标）
    property int highlightX: 0
    property int highlightY: 0
    property int highlightWidth: 0
    property int highlightHeight: 0

    Rectangle {
        visible: root.active
        x: root.highlightX
        y: root.highlightY
        width: root.highlightWidth
        height: root.highlightHeight
        radius: 3
        color: "#4a90e2"
        opacity: 0.75
        border.color: "#2c6cb0"
        border.width: 1

        Rectangle {
            anchors.centerIn: parent
            width: Math.max(6, parent.width / 3)
            height: width
            radius: 1
            color: "#ffffff"
            opacity: 0.9
        }
    }
}
