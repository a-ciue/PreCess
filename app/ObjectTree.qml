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
            let selectedModelId = App.selection.activeModelId
            let selectedComponentId = App.selection.activeComponentId
            treeModel.refresh()

            // TreeModel 重置后按业务 ID 恢复选择，目标已经删除时才清空。
            let idx = selectedComponentId >= 0
                    ? treeModel.findIndexByNodeId(selectedComponentId, 1)
                    : treeModel.findIndexByNodeId(selectedModelId, 0)
            if (idx.valid) {
                treeView.selectionModel.setCurrentIndex(idx, ItemSelectionModel.ClearAndSelect)
            } else {
                treeView.selectionModel.clear()
                App.selection.activeComponentId = -1
                App.selection.activeModelId = -1
            }
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

    Rectangle {
        id: headerBar
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 28
        color: "#f5f5f5"

        Row {
            anchors { right: parent.right; rightMargin: 6; verticalCenter: parent.verticalCenter }
            spacing: 2

            ToolButton {
                icon.source: "qrc:/images/modeltree/show.svg"
                icon.width: 20
                icon.height: 20
                implicitWidth: 26
                implicitHeight: 26
                display: ToolButton.IconOnly

                ToolTip.visible: hovered
                ToolTip.text: qsTr("显示全部")
                ToolTip.delay: 500

                onClicked: {
                    treeModel.setAllVisibility(true)
                    for (let i = 0; i < treeModel.rowCount(); i++) {
                        let idx = treeModel.index(i, 0)
                        App.modelVisibilityUpdated(
                            treeModel.data(idx, TreeModel.NodeIdRole), true)
                    }
                }
            }

            ToolButton {
                icon.source: "qrc:/images/modeltree/hide.svg"
                icon.width: 20
                icon.height: 20
                implicitWidth: 26
                implicitHeight: 26
                display: ToolButton.IconOnly

                ToolTip.visible: hovered
                ToolTip.text: qsTr("隐藏全部")
                ToolTip.delay: 500

                onClicked: {
                    treeModel.setAllVisibility(false)
                    for (let i = 0; i < treeModel.rowCount(); i++) {
                        let idx = treeModel.index(i, 0)
                        App.modelVisibilityUpdated(
                            treeModel.data(idx, TreeModel.NodeIdRole), false)
                    }
                }
            }
        }
    }

    TreeView {
        id: treeView
        anchors { left: parent.left; right: parent.right; top: headerBar.bottom; bottom: parent.bottom }
        model: treeModel
        columnSpacing: 0
        clip: true
        flickDeceleration: 100000
        boundsBehavior: Flickable.StopAtBounds
        columnWidthProvider: function(column) { return colWidth }
        property real colWidth: treeView.width

        property int toggleExpandRow: -1
        onExpanded: (row, depth) => toggleExpandRow = row
        onCollapsed: (row, recursively) => toggleExpandRow = row

        delegate: TreeViewDelegate {
            id: viewDelegate
            height: _rowHeight

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
                    color: viewDelegate.model.isVisible ? "black" : "#aaaaaa"
                    font.pixelSize: 10
                }
            }

            contentItem: RowLayout {
                spacing: 4

                Text {
                    id: nameText
                    Layout.fillWidth: true
                    Layout.maximumWidth: implicitWidth
                    Layout.leftMargin: viewDelegate._padding + 2
                    text: viewDelegate.model.name || "N/A"
                    color: viewDelegate.model.isVisible ? "black" : "#aaaaaa"
                    font.pixelSize: 13
                    font.family: "Consolas"
                    font.weight: viewDelegate.current ? Font.Bold : Font.Normal
                    font.italic: !viewDelegate.model.isVisible
                    elide: Text.ElideRight

                    ToolTip {
                        visible: nameText.truncated && viewDelegate.hovered
                        text: nameText.text
                        delay: 300
                    }
                }

                Text {
                    id: valueText
                    text: viewDelegate.model.number ? " (" + viewDelegate.model.number + ")" : ""
                    visible: text !== ""
                    color: viewDelegate.model.isVisible ? "black" : "#aaaaaa"
                    font.pixelSize: 11
                }

                Item {
                    Layout.fillWidth: true
                }

                Image {
                    id: eyeIcon
                    visible: viewDelegate.depth <= 1
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                    source: viewDelegate.model.isVisible
                        ? "qrc:/images/modeltree/show.svg"
                        : "qrc:/images/modeltree/hide.svg"
                    opacity: treeMouseArea._overEye ? 1.0 : 0.4

                    ToolTip.visible: treeMouseArea._overEye
                    ToolTip.text: viewDelegate.model.isVisible ? qsTr("隐藏") : qsTr("显示")
                    ToolTip.delay: 500
                }

                Item {
                    Layout.preferredWidth: 6
                }
            }

            MouseArea {
                id: treeMouseArea
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                hoverEnabled: true

                readonly property bool _overEye:
                    treeMouseArea.containsMouse
                    && viewDelegate.depth <= 1
                    && mouseX > treeMouseArea.width - 22

                cursorShape: treeMouseArea._overEye ? Qt.PointingHandCursor : Qt.ArrowCursor

                onClicked: (mouse) => {
                    if (treeMouseArea._overEye) {
                        if (mouse.button === Qt.RightButton) return
                        let idx = treeModel.findIndexByNodeId(viewDelegate.model.nodeId, viewDelegate.depth)
                        let newVis = !viewDelegate.model.isVisible
                        treeModel.setVisibility(idx, newVis)
                        if (viewDelegate.depth === 0)
                            App.modelVisibilityUpdated(viewDelegate.model.nodeId, newVis)
                        else
                            App.componentVisibilityUpdated(viewDelegate.model.nodeId, newVis)
                        return
                    }
                    if (mouse.button === Qt.RightButton) {
                        if (viewDelegate.depth > 1) return
                        contextMenu.popup()
                    } else {
                        let idx = treeModel.findIndexByNodeId(viewDelegate.model.nodeId, viewDelegate.depth)
                        let compId = viewDelegate.model.componentId

                        if (viewDelegate.current) {
                            viewDelegate.treeView.selectionModel.clear()
                            App.selection.activeComponentId = -1
                            App.selection.activeModelId = -1
                        } else if (viewDelegate.depth === 0 && idx.valid) {
                            // Model 节点没有 componentId，直接使用自身 nodeId。
                            viewDelegate.treeView.selectionModel.setCurrentIndex(idx, ItemSelectionModel.ClearAndSelect)
                            App.selection.activeComponentId = -1
                            App.selection.activeModelId = viewDelegate.model.nodeId
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
                        treeModel.setVisibility(idx, false)
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
                        treeModel.setVisibility(idx, true)
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
                        App.selection.activeComponentId = -1
                        App.selection.activeModelId = -1
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
