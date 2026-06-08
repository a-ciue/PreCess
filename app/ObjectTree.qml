import QtQuick 6.3
import QtQuick.Controls 2.15
import QtQuick.Controls.Basic
import QtQuick.Layouts 1.15

Pane{
    id: objectTree
    padding: 0

    required property var modelQuery
    property int curModelId: -1

    signal selectionChanged(int modelId)
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
        delegate: TreeViewDelegate {
            id:viewDelegate
            width: treeView.width
            height: 30

            readonly property real _padding: 5
            readonly property real _rowHeight: 25
            readonly property real _indentWidth: 20

            background: Rectangle {
                anchors.fill: parent
                color: viewDelegate.hovered ? "#f0f0f0" : "transparent"
            }

            // 展开/折叠指示器
            indicator: Rectangle {
                id: indicatorItem
                x: viewDelegate._padding + viewDelegate.depth * viewDelegate._indentWidth
                y: (viewDelegate._rowHeight - implicitHeight) / 2

                implicitWidth: 16
                implicitHeight: 16
                color: "transparent"
                visible: viewDelegate.isTreeNode && viewDelegate.hasChildren
                z: 10

                rotation: viewDelegate.expanded ? 0 : -90
                Behavior on rotation {
                    NumberAnimation { duration: 100 }
                }

                Text {
                    anchors.centerIn: parent
                    text: "▼"
                    color: "#666666"
                    font.pixelSize: 10
                }

                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    onTapped: {
                        viewDelegate.treeView.toggleExpanded(viewDelegate.row)
                    }
                }
            }

            // 内容区域
            contentItem: Item {
                implicitWidth: textContent.implicitWidth + viewDelegate._padding
                implicitHeight: viewDelegate._rowHeight

                // 左键选中（仅限 contentItem 区域，不触发展开）
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    onClicked: (mouse) => {
                        mouse.accepted = true
                        let idx = treeModel.findIndexByNodeId(viewDelegate.model.nodeId, viewDelegate.depth)
                        if (viewDelegate.current) {
                            viewDelegate.treeView.selectionModel.clear()
                            objectTree.curModelId = -1
                            objectTree.selectionChanged(-1)
                        } else if (idx.valid) {
                            viewDelegate.treeView.selectionModel.setCurrentIndex(idx, ItemSelectionModel.ClearAndSelect)
                            objectTree.curModelId = viewDelegate.model.modelId
                            objectTree.selectionChanged(viewDelegate.model.modelId)
                        }
                    }
                }

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
                            return viewDelegate.current ? "#333333" : "#555555"
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
                        color: "#888888"
                        font.pixelSize: 11
                        visible: text !== ""
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            // 右键菜单
            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: {
                    if (viewDelegate.depth > 1) return
                    let idx = treeModel.findIndexByNodeId(viewDelegate.model.nodeId, viewDelegate.depth)
                    viewDelegate.treeView.selectionModel.setCurrentIndex(idx, ItemSelectionModel.ClearAndSelect)
                    contextMenu.popup()
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
