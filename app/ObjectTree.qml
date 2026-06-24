import QtQuick 6.3
import QtQuick.Controls 2.15
import QtQuick.Controls.Basic
import QtQuick.Layouts 1.15

import app.core
import app.model

Pane {
    id: objectTree
    padding: 0
    clip: true

    TreeModel {
        id: treeModel
        modelQuery: QModelManager.query
    }

    Timer {
        id: refreshTimer
        interval: 100
        repeat: false
        onTriggered: {
            treeModel.refresh()
            App.selection.activeComponentId = -1
            App.selection.activeModelId = -1
        }
    }

    Connections {
        target: QModelManager.observer
        function onModelAdded(modelId)   { refreshTimer.restart() }
        function onModelChanged(modelId) { refreshTimer.restart() }
        function onModelRemoved(modelId) { refreshTimer.restart() }
        function onComponentRemoved(componentId) { refreshTimer.restart() }
        function onComponentChanged(componentId) { refreshTimer.restart() }
    }

    TreeView {
        id: treeView
        anchors.fill: parent
        model: treeModel
        columnSpacing: 0
        clip: true
        flickDeceleration: 100000
        boundsBehavior: Flickable.StopAtBounds
        columnWidthProvider: function(column) { return treeView.width }

        property int toggleExpandRow: -1
        onExpanded: (row, depth) => toggleExpandRow = row
        onCollapsed: (row, recursively) => toggleExpandRow = row

        delegate: TreeViewDelegate {
            id: viewDelegate
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
                id: ctItem
                implicitHeight: viewDelegate._rowHeight

                Text {
                    id: valueText
                    anchors.verticalCenter: parent.verticalCenter
                    visible: text !== ""
                    text: viewDelegate.model.number ? " (" + viewDelegate.model.number + ")" : ""
                    color: "black"
                    font.pixelSize: 11

                    x: nameText.x + nameText.width + 4
                }

                Text {
                    id: nameText
                    anchors.verticalCenter: parent.verticalCenter
                    x: viewDelegate._padding + 2
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

                    readonly property real _scrollMargin: 10
                    readonly property real _avail: ctItem.width - x - (valueText.visible ? valueText.width + 4 : 0) - _scrollMargin
                    width: Math.min(implicitWidth, Math.max(0, _avail))
                }
            }

            ToolTip {
                id: detailTooltip
                visible: viewDelegate.hovered && nameText.truncated
                text: nameText.text + (valueText.text ? " " + valueText.text : "")
                delay: 300
                z: 9999

                contentItem: Text {
                    text: detailTooltip.text
                    color: "black"
                    font.family: "Consolas"
                    font.pixelSize: 13
                }

                background: Rectangle {
                    color: "white"
                    border.color: "#c0c0c0"
                    border.width: 1
                    radius: 4
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
                            App.selection.activeComponentId = -1
                            App.selection.activeModelId = -1
                        } else if (idx.valid) {
                            viewDelegate.treeView.selectionModel.setCurrentIndex(idx, ItemSelectionModel.ClearAndSelect)
                            App.selection.activeComponentId = compId
                            App.selection.activeModelId = QModelManager.query.findModelIdByComponent(compId)
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
                        if (viewDelegate.depth === 0)
                            App.modelVisibilityUpdated(viewDelegate.model.nodeId, false)
                        else
                            App.componentVisibilityUpdated(viewDelegate.model.nodeId, false)
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
                        if (viewDelegate.depth === 0)
                            App.modelVisibilityUpdated(viewDelegate.model.nodeId, true)
                        else
                            App.componentVisibilityUpdated(viewDelegate.model.nodeId, true)
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
                        if (viewDelegate.depth === 0)
                            QModelManager.removeModel(viewDelegate.model.nodeId)
                        else
                            QModelManager.removeComponent(viewDelegate.model.nodeId)
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

    Component.onCompleted: {
        App.registry.treeModel = treeModel
    }
}
