/**
 * @file PanelGroup.qml
 * @brief 分组视图：单行标题/标签栏 + 内容宿主
 *
 * 多标签分组为标签模式（标签即标题、顶满行高、每个标签可单独关闭）；
 * 单标签分组为标题模式（面板标题 + 按钮，与原来一致）。
 * 行内空白拖动整个分组；中央持久分组不显示该行。
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
    clip: true

    readonly property bool showTitleBar: groupView ? groupView.hasTitleBar : false
    readonly property bool closable: groupView ? groupView.closable : false
    readonly property bool floatable: groupView ? groupView.floatable : false
    //! @brief 多标签 → 标签模式；单标签 → 标题模式
    readonly property bool tabMode: groupView ? groupView.tabCount > 1 : false

    // 单行标题/标签栏：多标签时是标签行，单标签时是标题行；行内空白拖动整组
    Rectangle {
        id: titleBar
        objectName: "titleBar"
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            leftMargin: 1
            rightMargin: 1
        }
        height: visible ? 30 : 0
        visible: root.showTitleBar
        color: "#eff0f1"

        // 行内空白：拖动整个分组 / 右键分组菜单
        MouseArea {
            id: titleDragArea
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onPressed: function(mouse) {
                if (mouse.button === Qt.RightButton) {
                    titleMenu.popup()
                    return
                }
                root.groupView.beginGroupDrag(mapToGlobal(mouse.x, mouse.y))
            }
            onPositionChanged: function(mouse) {
                if (pressed)
                    root.groupView.dragTo(mapToGlobal(mouse.x, mouse.y))
            }
            onReleased: function(mouse) {
                if (mouse.button === Qt.LeftButton)
                    root.groupView.endDrag(mapToGlobal(mouse.x, mouse.y))
            }
        }

        // 标题模式（单标签）：显示面板标题（原形态）
        Text {
            objectName: "titleText"
            visible: !root.tabMode
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

        // 标签模式（多标签）：标签即标题，顶满行高；宽度按内容收缩，空白留给整组拖动
        TabBar {
            id: tabBar
            objectName: "tabBar"
            anchors {
                left: parent.left
                leftMargin: 1
                top: parent.top
                bottom: parent.bottom
            }
            visible: root.tabMode
            width: Math.min(implicitWidth, Math.max(0, parent.width - buttonsRow.width - 16))
            clip: true
            position: TabBar.Header

            // 当前下标由 C++ 在模型同步后校正（syncTabIndex）；此处仅处理用户触发的切换
            onCurrentIndexChanged: {
                if (!root.groupView || root.groupView.updatingTabs)
                    return
                if (root.groupView.activeIndex !== currentIndex)
                    root.groupView.activateTab(currentIndex)
            }

            Repeater {
                model: root.groupView ? root.groupView.tabNames : []

                TabButton {
                    id: tabButton
                    required property string modelData
                    required property int index

                    text: modelData
                    height: tabBar.height
                    width: Math.max(80, implicitWidth + (root.closable ? 34 : 16))

                    ToolTip.visible: tabMouse.containsMouse
                    ToolTip.text: tabButton.text

                    MouseArea {
                        id: tabMouse
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
                        hoverEnabled: true

                        onPressed: function(mouse) {
                            if (mouse.button === Qt.RightButton) {
                                tabMenu.popup()
                                return
                            }
                            if (mouse.button !== Qt.LeftButton)
                                return
                            root.groupView.beginPanelDrag(index, mapToGlobal(mouse.x, mouse.y))
                        }
                        onPositionChanged: function(mouse) {
                            if (pressed)
                                root.groupView.dragTo(mapToGlobal(mouse.x, mouse.y))
                        }
                        onReleased: function(mouse) {
                            if (mouse.button === Qt.LeftButton)
                                root.groupView.endDrag(mapToGlobal(mouse.x, mouse.y))
                        }
                    }

                    // 标签关闭按钮：所有标签常显，可单独关闭
                    Rectangle {
                        objectName: "tabClose"
                        anchors {
                            right: parent.right
                            rightMargin: 3
                            verticalCenter: parent.verticalCenter
                        }
                        width: 14
                        height: 14
                        radius: 2
                        visible: root.closable
                        color: tabCloseArea.containsMouse ? "#e81123" : "transparent"

                        Rectangle {
                            anchors.centerIn: parent
                            width: 8
                            height: 1
                            rotation: 45
                            color: tabCloseArea.containsMouse ? "#ffffff" : "#4d4d4d"
                        }
                        Rectangle {
                            anchors.centerIn: parent
                            width: 8
                            height: 1
                            rotation: -45
                            color: tabCloseArea.containsMouse ? "#ffffff" : "#4d4d4d"
                        }
                        MouseArea {
                            id: tabCloseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: root.groupView.hidePanelAt(tabButton.index)
                        }
                    }

                    // 标签右键菜单（浮动/回停、关闭、关闭其他）
                    Menu {
                        id: tabMenu
                        MenuItem {
                            text: root.groupView && root.groupView.detached ? qsTr("回停") : qsTr("浮动")
                            enabled: root.floatable
                            onTriggered: root.groupView.toggleDetached()
                        }
                        MenuSeparator {}
                        MenuItem {
                            text: qsTr("关闭")
                            enabled: root.closable
                            onTriggered: root.groupView.hidePanelAt(tabButton.index)
                        }
                        MenuItem {
                            text: qsTr("关闭其他")
                            enabled: root.closable && tabBar.count > 1
                            onTriggered: root.groupView.hideOthers(tabButton.index)
                        }
                    }
                }
            }
        }

        Row {
            id: buttonsRow
            objectName: "titleButtons"
            anchors {
                right: parent.right
                rightMargin: 4
                verticalCenter: parent.verticalCenter
            }
            spacing: 2
            visible: root.groupView && !root.groupView.central

            // 标签列表下拉（标签过多时快速定位；对齐 ADS TabsMenu）
            Rectangle {
                width: 20
                height: 20
                radius: 2
                visible: root.groupView && root.groupView.tabCount > 1
                color: tabsMenuArea.containsMouse ? "#d8d8d8" : "transparent"

                Column {
                    anchors.centerIn: parent
                    spacing: 3
                    Repeater {
                        model: 3
                        Rectangle {
                            width: 11
                            height: 1
                            color: "#4d4d4d"
                        }
                    }
                }
                MouseArea {
                    id: tabsMenuArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: tabsMenu.popup()
                }
                Menu {
                    id: tabsMenu
                    Instantiator {
                        model: root.groupView ? root.groupView.tabNames : []
                        delegate: MenuItem {
                            required property string modelData
                            required property int index
                            text: modelData
                            checkable: true
                            checked: root.groupView && root.groupView.activeIndex === index
                            onTriggered: root.groupView.activateTab(index)
                        }
                        onObjectAdded: function(index, object) { tabsMenu.insertItem(index, object) }
                        onObjectRemoved: function(index, object) { tabsMenu.removeItem(object) }
                    }
                }
            }

            // 浮动 / 回停
            Rectangle {
                width: 20
                height: 20
                radius: 2
                visible: root.floatable
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
                visible: root.closable
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

        // 标题栏右键菜单（浮动/回停、关闭分组、关闭其他分组）
        Menu {
            id: titleMenu
            MenuItem {
                text: root.groupView && root.groupView.detached ? qsTr("回停分组") : qsTr("浮动分组")
                enabled: root.floatable
                onTriggered: root.groupView.toggleDetached()
            }
            MenuSeparator {}
            MenuItem {
                text: qsTr("关闭分组")
                enabled: root.closable
                onTriggered: root.groupView.hideGroup()
            }
            MenuItem {
                text: qsTr("关闭其他分组")
                enabled: root.closable
                onTriggered: root.groupView.hideOtherGroups()
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
            top: titleBar.visible ? titleBar.bottom : parent.top
            bottom: parent.bottom
            margins: 1
        }
        clip: true
    }

    // 组内标签重排插入标记（拖拽标签横向移动时显示，释放时提交顺序）
    Rectangle {
        visible: root.groupView ? root.groupView.reordering : false
        x: root.groupView ? root.groupView.reorderMarkerX - 1 : 0
        y: titleBar.y + tabBar.y
        width: 2
        height: tabBar.height
        color: "#2f6fb5"
        z: 3
    }
}
