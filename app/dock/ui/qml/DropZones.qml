/**
 * @file DropZones.qml
 * @brief 拖放落点方框（在透明顶层指示器窗口中绘制）
 *
 * 同时显示全部候选落点方框，当前命中的一个加深高亮。
 */

import QtQuick
import PreCess.Docking as Docking

Item {
    id: root

    //! @brief 指示器列表：[{x, y, width, height, active}]
    property var zones: []
    //! @brief 目标分组描边：[{x, y, width, height}]，无效时不描边
    property var targetFrame: null
    //! @brief 标签插入标记：[{x, y, width, height}]，无效时不显示
    property var tabInsert: null

    // 落点配色：取自 Docking.Theme.colors（与 app/Theme.qml 同步）
    readonly property color accent: Docking.Theme.colors.dropAccent
    readonly property color zoneFill: Docking.Theme.colors.dropZoneFill
    readonly property color zoneBorder: Docking.Theme.colors.dropZoneBorder
    readonly property color zoneMarker: Docking.Theme.colors.dropZoneMarker

    // 标签插入标记（悬停目标分组的标题栏/标签栏条带时显示竖线）
    Rectangle {
        visible: root.tabInsert !== null && root.tabInsert !== undefined
            && root.tabInsert.width > 0
        x: root.tabInsert ? root.tabInsert.x : 0
        y: root.tabInsert ? root.tabInsert.y : 0
        width: root.tabInsert ? root.tabInsert.width : 0
        height: root.tabInsert ? root.tabInsert.height : 0
        color: root.accent
        radius: 1
    }

    // 目标分组描边（拖动悬停在哪一面板，哪一面板整体高亮）
    Rectangle {
        visible: root.targetFrame !== null && root.targetFrame !== undefined
            && root.targetFrame.width > 0
        x: root.targetFrame ? root.targetFrame.x : 0
        y: root.targetFrame ? root.targetFrame.y : 0
        width: root.targetFrame ? root.targetFrame.width : 0
        height: root.targetFrame ? root.targetFrame.height : 0
        color: "transparent"
        border.color: root.accent
        border.width: 2
        radius: 2
    }

    Repeater {
        model: root.zones

        Rectangle {
            required property var modelData

            x: modelData.x
            y: modelData.y
            width: modelData.width
            height: modelData.height
            radius: 3
            color: modelData.active ? root.accent : root.zoneFill
            opacity: modelData.active ? 0.9 : 0.6
            border.color: modelData.active ? "#ffffff" : root.zoneBorder
            border.width: modelData.active ? 2 : 1

            // 位置标记：中心方框画大方块，其余画小方块
            Rectangle {
                anchors.centerIn: parent
                width: Math.max(6, parent.width / 3)
                height: width
                radius: 1
                color: modelData.active ? "#ffffff" : root.zoneMarker
                opacity: 0.9
            }
        }
    }
}
