/**
 * @file DockGroup.qml
 * @brief 分组视图：标题栏 + 选项卡栏 + 内容宿主
 */

import QtQuick
import PreCess.Docking as Docking

Rectangle {
    id: root

    //! @brief 关联的 GroupView（C++）
    property var groupView: null

    color: "transparent"
    border.color: "#b8b8b8"
    border.width: 1

    readonly property bool hasTabs: groupView ? groupView.tabCount > 1 : false
    readonly property bool showTitleBar: groupView ? groupView.titleBarVisible : false

    Item {
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

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            onPressed: function(mouse) { root.groupView.beginDrag(root.mapToGlobal(mouse.x, mouse.y)) }
            onPositionChanged: function(mouse) { if (pressed) root.groupView.dragTo(root.mapToGlobal(mouse.x, mouse.y)) }
            onReleased: function(mouse) { root.groupView.endDrag(root.mapToGlobal(mouse.x, mouse.y)) }
            onDoubleClicked: root.groupView.toggleFloat()
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
            Item {
                width: 20
                height: 20
                Rectangle {
                    anchors.centerIn: parent
                    width: 11
                    height: 11
                    color: "transparent"
                    border.width: 1
                    border.color: floatArea.containsMouse ? "#1a6fc4" : "#555555"
                }
                Rectangle {
                    visible: root.groupView && root.groupView.floating
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: 6
                    width: 11
                    height: 2
                    color: floatArea.containsMouse ? "#1a6fc4" : "#555555"
                }
                MouseArea {
                    id: floatArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.groupView.toggleFloat()
                }
            }

            // 关闭
            Item {
                width: 20
                height: 20
                Rectangle {
                    anchors.centerIn: parent
                    width: 12
                    height: 1
                    rotation: 45
                    color: closeArea.containsMouse ? "#c0392b" : "#555555"
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: 12
                    height: 1
                    rotation: -45
                    color: closeArea.containsMouse ? "#c0392b" : "#555555"
                }
                MouseArea {
                    id: closeArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.groupView.closeGroup()
                }
            }
        }
    }

    Item {
        id: tabBar
        objectName: "tabBar"
        anchors {
            left: parent.left
            right: parent.right
            top: titleBar.bottom
            margins: 1
        }
        height: visible ? 28 : 0
        visible: root.hasTabs

        Row {
            anchors.fill: parent

            Repeater {
                model: root.groupView ? root.groupView.tabTitles : []

                Rectangle {
                    required property string modelData
                    required property int index

                    width: Math.max(80, tabText.implicitWidth + 20)
                    height: parent.height
                    color: index === root.groupView.currentIndex ? "#ffffff" : "#e4e6e8"
                    border.color: "#b8b8b8"
                    border.width: 1

                    Text {
                        id: tabText
                        anchors.centerIn: parent
                        text: parent.modelData
                        font.pixelSize: 12
                        color: "#333333"
                    }

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton
                        onPressed: function(mouse) {
                            root.groupView.setCurrentIndex(index)
                            root.groupView.beginDrag(root.mapToGlobal(mouse.x, mouse.y))
                        }
                        onPositionChanged: function(mouse) {
                            if (pressed)
                                root.groupView.dragTo(root.mapToGlobal(mouse.x, mouse.y))
                        }
                        onReleased: function(mouse) { root.groupView.endDrag(root.mapToGlobal(mouse.x, mouse.y)) }
                    }
                }
            }
        }
    }

    // 内容宿主：C++ 侧把当前面板的 guest item 重挂到此处
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
