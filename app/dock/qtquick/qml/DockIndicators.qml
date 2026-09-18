/**
 * @file DockIndicators.qml
 * @brief 拖放落点方框（在透明顶层指示器窗口中绘制）
 *
 * 同时显示全部候选落点方框，当前命中的一个加深高亮。
 */

import QtQuick

Item {
    id: root

    //! @brief 指示器列表：[{x, y, width, height, active}]
    property var indicators: []

    Repeater {
        model: root.indicators

        Rectangle {
            required property var modelData

            x: modelData.x
            y: modelData.y
            width: modelData.width
            height: modelData.height
            radius: 3
            color: modelData.active ? "#2f6fb5" : "#cfe0f3"
            opacity: modelData.active ? 0.9 : 0.6
            border.color: modelData.active ? "#ffffff" : "#8fb2d9"
            border.width: modelData.active ? 2 : 1

            // 位置标记：中心方框画大方块，其余画小方块
            Rectangle {
                anchors.centerIn: parent
                width: Math.max(6, parent.width / 3)
                height: width
                radius: 1
                color: modelData.active ? "#ffffff" : "#9fb9d6"
                opacity: 0.9
            }
        }
    }
}
