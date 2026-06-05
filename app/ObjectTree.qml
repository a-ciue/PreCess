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
                color: {
                    if (viewDelegate.current) {
                        return "#e3f2fd"
                    } else if (viewDelegate.hovered) {
                        return "#f0f0f0"
                    } else {
                        return "transparent"
                    }
                }

                Rectangle {
                    width: 3
                    height: parent.height
                    color: viewDelegate.current ? "#1976d2" : "transparent"
                    anchors.left: parent.left
                }
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

                Text {
                    anchors.centerIn: parent
                    text: viewDelegate.expanded ? "▼" : "▶"
                    color: "#666666"
                    font.pixelSize: 10

                    Behavior on rotation {
                        NumberAnimation { duration: 100 }
                    }
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
                implicitWidth: textContent.implicitWidth + viewDelegate._padding * 2
                implicitHeight: viewDelegate._rowHeight

                Row {
                    id: textContent
                    anchors.fill: parent
                    anchors.leftMargin: viewDelegate._padding + (viewDelegate.hasChildren ? indicatorItem.implicitWidth + 2 : 0)
                    anchors.rightMargin: viewDelegate._padding
                    spacing: 4

                    Text {
                        id: nameText
                        text: viewDelegate.model.name || "N/A"
                        color: {
                            if (!viewDelegate.model.isVisible) {
                                return "#aaaaaa"
                            } else if (viewDelegate.current) {
                                return "#1976d2"
                            } else {
                                return "#333333"
                            }
                        }
                        font.pixelSize: 13
                        font.family: "Consolas"
                        font.weight: viewDelegate.current ? Font.Bold : Font.Normal
                        font.italic: !viewDelegate.model.isVisible
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                        height: parent.height
                    }

                    Text {
                        id: valueText
                        text: viewDelegate.model.number ? " (" + viewDelegate.model.number + ")" : ""
                        color: "#888888"
                        font.pixelSize: 11
                        visible: text !== ""
                        verticalAlignment: Text.AlignVCenter
                        height: parent.height
                    }
                }
            }

            // 左键点击选择
            TapHandler {
                acceptedButtons: Qt.LeftButton
                onTapped: {
                    let index = viewDelegate.treeView.index(viewDelegate.row, viewDelegate.column)
                    viewDelegate.treeView.selectionModel.setCurrentIndex(index, ItemSelectionModel.ClearAndSelect)

                    objectTree.curModelId = viewDelegate.model.modelId
                    objectTree.selectionChanged(viewDelegate.model.modelId)
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
