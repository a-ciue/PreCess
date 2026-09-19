/**
 * @file PanelGroup.qml
 * @brief 分组视图：标题栏 + 标签栏 + 内容宿主
 */

import QtQuick
import QtQuick.Controls
import PreCess.Docking as Docking

Rectangle {
    id: root

    //! @brief 关联的 PanelGroupItem（C++）
    property var groupView: null

    color: "transparent"
    border.color: "#b8b8b8"
    border.width: 1
    radius: 2

    readonly property bool hasTabs: groupView ? groupView.tabCount > 1 : false
    readonly property bool showTitleBar: groupView ? groupView.hasTitleBar : false

    // 标题栏：常显，标题 + 浮动/关闭按钮，整栏可拖动
    Rectangle {
        id: titleBar
        objectName: "titleBar"
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: 1
        }
        height: visible ? 30 : 0
        visible: root.showTitleBar
        color: "#eff0f1"

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            onPressed: function(mouse) { root.groupView.beginGroupDrag(mapToGlobal(mouse.x, mouse.y)) }
            onPositionChanged: function(mouse) {
                if (pressed)
                    root.groupView.dragTo(mapToGlobal(mouse.x, mouse.y))
            }
            onReleased: function(mouse) { root.groupView.endDrag(mapToGlobal(mouse.x, mouse.y)) }
            onDoubleClicked: root.groupView.toggleDetached()
        }

        Text {
            anchors {
                left: parent.left
                leftMargin: 6
                right: buttonsRow.left
                rightMargin: 6
                verticalCenter: parent.verticalCenter
            }
            text: root.groupView ? root.groupView.title : ""
            elide: Text.ElideRight
            font.pixelSize: 12
            color: "#333333"
        }

        Row {
            id: buttonsRow
            anchors {
                right: parent.right
                rightMargin: 4
                verticalCenter: parent.verticalCenter
            }
            spacing: 2
            visible: root.groupView && !root.groupView.central

            // 浮动 / 回停
            Rectangle {
                width: 20
                height: 20
                radius: 2
                color: floatArea.containsMouse ? "#d8d8d8" : "transparent"

                Rectangle {
                    anchors.centerIn: parent
                    width: 11
                    height: 11
                    color: "transparent"
                    border.width: 1
                    border.color: "#4d4d4d"
                }
                Rectangle {
                    visible: root.groupView && root.groupView.detached
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        bottom: parent.bottom
                        bottomMargin: 3
                    }
                    width: 11
                    height: 2
                    color: "#4d4d4d"
                }
                MouseArea {
                    id: floatArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.groupView.toggleDetached()
                }
            }

            // 关闭
            Rectangle {
                width: 20
                height: 20
                radius: 2
                color: closeArea.containsMouse ? "#e81123" : "transparent"

                Rectangle {
                    anchors.centerIn: parent
                    width: 12
                    height: 1
                    rotation: 45
                    color: closeArea.containsMouse ? "#ffffff" : "#4d4d4d"
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: 12
                    height: 1
                    rotation: -45
                    color: closeArea.containsMouse ? "#ffffff" : "#4d4d4d"
                }
                MouseArea {
                    id: closeArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.groupView.hideGroup()
                }
            }
        }
    }

    // 标签栏：QtQuick Controls TabBar，跟随应用控件样式；拖标签分离单个面板
    TabBar {
        id: tabBar
        objectName: "tabBar"
        anchors {
            left: parent.left
            right: parent.right
            top: titleBar.visible ? titleBar.bottom : parent.top
            margins: 1
        }
        height: visible ? implicitHeight : 0
        visible: root.hasTabs
        position: TabBar.Header
        currentIndex: root.groupView ? root.groupView.activeIndex : -1

        onCurrentIndexChanged: {
            if (root.groupView && root.groupView.activeIndex !== currentIndex)
                root.groupView.activateTab(currentIndex)
        }

        Repeater {
            model: root.groupView ? root.groupView.tabNames : []

            TabButton {
                id: tabButton
                required property string modelData
                required property int index

                text: modelData
                width: Math.max(80, implicitWidth + 16)

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    onPressed: function(mouse) {
                        root.groupView.beginPanelDrag(index, mapToGlobal(mouse.x, mouse.y))
                    }
                    onPositionChanged: function(mouse) {
                        if (pressed)
                            root.groupView.dragTo(mapToGlobal(mouse.x, mouse.y))
                    }
                    onReleased: function(mouse) {
                        root.groupView.endDrag(mapToGlobal(mouse.x, mouse.y))
                    }
                }
            }
        }
    }

    // 内容宿主：C++ 侧把当前面板的 client item 重挂到此处
    Item {
        id: contentArea
        objectName: "contentArea"
        anchors {
            left: parent.left
            right: parent.right
            top: tabBar.visible ? tabBar.bottom : (titleBar.visible ? titleBar.bottom : parent.top)
            bottom: parent.bottom
            margins: 1
        }
    }
}
