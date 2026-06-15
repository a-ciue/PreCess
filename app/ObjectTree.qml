import QtQuick 6.3
import QtQuick.Controls 2.15
import QtQuick.Controls.Basic
import QtQuick.Layouts 1.15

Pane{
    id: objectTree
    padding: 0

    required property var modelQuery
    property int curModelId: -1
    property int curComponentId: -1

    signal selectionChanged(int componentId)
    signal deleteRequested(int nodeId, int depth)
    signal visibilityChanged(int nodeId, int depth, bool visible)

    TreeModel {
        id: treeModel
        modelQuery: objectTree.modelQuery
    }

    function refreshTree() {
        treeModel.refresh()
    }

    TreeView {
        id:treeView
        anchors.fill: parent
        model: treeModel
        columnSpacing: 0

        property int toggleExpandRow: -1
        onExpanded: (row, depth) => toggleExpandRow = row
        onCollapsed: (row, recursively) => toggleExpandRow = row

        delegate: TreeViewDelegate {
            id:viewDelegate
            width: treeView.width
            height: 20

            readonly property real _padding: 5
            readonly property real _rowHeight: 18
            readonly property real _indentWidth: 20

            TableView.onPooled: indicatorAnim.complete()
            TableView.onReused: {
                if (treeView.toggleExpandRow === viewDelegate.row) {
                    treeView.toggleExpandRow = -1
                    indicatorAnim.start()
                }
            }

            background: Rectangle {
                anchors.fill: parent
                color: viewDelegate.hovered ? "#f0f0f0" : "transparent"
            }

            // 展开/折叠指示器
            indicator: Rectangle {
                id: indicatorItem
                x: viewDelegate._padding + viewDelegate.depth * viewDelegate._indentWidth
                anchors.verticalCenter: parent.verticalCenter

                implicitWidth: 16
                implicitHeight: 16
                color: "transparent"
                z: 10

                Binding on rotation {
                    when: !indicatorAnim.running
                    value: viewDelegate.expanded ? 0 : -90
                }

                NumberAnimation {
                    id: indicatorAnim
                    target: indicatorItem
                    property: "rotation"
                    from: viewDelegate.expanded ? -90 : 0
                    to: viewDelegate.expanded ? 0 : -90
                    duration: 200
                    easing.type: Easing.OutQuart
                }

                Text {
                    anchors.centerIn: parent
                    text: "▼"
                    color: "black"
                    font.pixelSize: 10
                }
            }

            // 内容区域
            contentItem: Item {
                implicitWidth: textContent.implicitWidth + viewDelegate._padding
                implicitHeight: viewDelegate._rowHeight

                Row {
                    id: textContent
                    anchors.left: parent.left
                    anchors.leftMargin: viewDelegate._padding + 2
                    anchors.right: parent.right
                    anchors.rightMargin: viewDelegate._padding
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4

                    Text {
                        id: nameText
                        text: viewDelegate.model.name || "N/A"
                        color: {
                            if (!viewDelegate.model.isVisible) return "#aaaaaa"
                            return "black"
                        }
                        font.pixelSize: 13
                        font.family: "Consolas"
                        font.weight: viewDelegate.current ? Font.Bold : Font.Normal
                        font.italic: !viewDelegate.model.isVisible
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }

                    Text {
                        id: valueText
                        text: viewDelegate.model.number ? " (" + viewDelegate.model.number + ")" : ""
                        color: "black"
                        font.pixelSize: 11
                        visible: text !== ""
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            // 统一交互区
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: (mouse) => {
                    if (mouse.button === Qt.RightButton) {
                        if (viewDelegate.depth > 1) return
                        let idx = treeModel.findIndexByNodeId(viewDelegate.model.nodeId, viewDelegate.depth)
                        viewDelegate.treeView.selectionModel.setCurrentIndex(idx, ItemSelectionModel.ClearAndSelect)
                        contextMenu.popup()
                    } else {
                        let idx = treeModel.findIndexByNodeId(viewDelegate.model.nodeId, viewDelegate.depth)
                        let compId = viewDelegate.model.componentId

                        if (viewDelegate.current || compId < 0) {
                            viewDelegate.treeView.selectionModel.clear()
                            objectTree.curModelId = -1
                            objectTree.curComponentId = -1
                            objectTree.selectionChanged(-1)
                        } else if (idx.valid) {
                            viewDelegate.treeView.selectionModel.setCurrentIndex(idx, ItemSelectionModel.ClearAndSelect)
                            objectTree.curComponentId = compId
                            objectTree.curModelId = modelQuery.findModelIdByComponent(compId)
                            objectTree.selectionChanged(compId)
                        }
                    }
                }
            }

            Menu {
                id: contextMenu

                background: Rectangle {
                    color: "#ffffff"
                    border.color: "#d0d0d0"
                    border.width: 1
                    radius: 4
                    implicitWidth: 100
                }

                MenuItem {
                    id: hideItem
                    text: "隐藏"
                    implicitHeight: 30

                    background: Rectangle {
                        color: hideItem.hovered ? "#f0f0f0" : "transparent"
                    }

                    contentItem: Text {
                        text: hideItem.text
                        color: hideItem.hovered ? "#1976d2" : "#333333"
                        font.pixelSize: 12
                        font.family: "Microsoft YaHei"
                        leftPadding: 15
                        verticalAlignment: Text.AlignVCenter
                    }

                    onTriggered: {
                        let idx = treeModel.findIndexByNodeId(viewDelegate.model.nodeId, viewDelegate.depth)
                        treeModel.setVisibility(idx.row, idx.parent, false)
                        objectTree.visibilityChanged(viewDelegate.model.nodeId, viewDelegate.depth, false)
                    }
                }

                MenuItem {
                    id: showItem
                    text: "显示"
                    implicitHeight: 30

                    background: Rectangle {
                        color: showItem.hovered ? "#f0f0f0" : "transparent"
                    }

                    contentItem: Text {
                        text: showItem.text
                        color: showItem.hovered ? "#1976d2" : "#333333"
                        font.pixelSize: 12
                        font.family: "Microsoft YaHei"
                        leftPadding: 15
                        verticalAlignment: Text.AlignVCenter
                    }

                    onTriggered: {
                        let idx = treeModel.findIndexByNodeId(viewDelegate.model.nodeId, viewDelegate.depth)
                        treeModel.setVisibility(idx.row, idx.parent, true)
                        objectTree.visibilityChanged(viewDelegate.model.nodeId, viewDelegate.depth, true)
                    }
                }

                MenuSeparator {}

                MenuItem {
                    id: deleteItem
                    text: "删除"
                    implicitHeight: 30

                    background: Rectangle {
                        color: deleteItem.hovered ? "#ffebee" : "transparent"
                    }

                    contentItem: Text {
                        text: deleteItem.text
                        color: deleteItem.hovered ? "#d32f2f" : "#c62828"
                        font.pixelSize: 12
                        font.family: "Microsoft YaHei"
                        leftPadding: 15
                        verticalAlignment: Text.AlignVCenter
                    }

                    onTriggered: {
                        viewDelegate.treeView.selectionModel.clear()
                        contextMenu.close()
                        objectTree.deleteRequested(
                            viewDelegate.model.nodeId,
                            viewDelegate.depth
                        )
                    }
                }
            }
        }

        selectionModel: ItemSelectionModel {}

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            background: Rectangle {
                implicitWidth: 8
                color: "transparent"
            }
            contentItem: Rectangle {
                implicitWidth: 8
                radius: 4
                color: parent.hovered ? "#c0c0c0" : "#e0e0e0"
            }
        }
    }
}
