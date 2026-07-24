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

    // 选择 Model 时默认关联其第一个 Component，二者共同组成当前操作目标。
    function firstComponentId(modelId) {
        let components = QModelManager.query.getComponentsSummary(modelId)
        return components.length > 0 ? components[0].component_id : -1
    }

    readonly property int _nodeTypeMesh: 2
    readonly property int _nodeTypeGeometry: 3

    Timer {
        id: refreshTimer
        interval: 100
        repeat: false
        onTriggered: {
            treeModel.refresh()
            // 选中项仍然有效时保留，不重置为 -1
            let compId = App.selection.activeComponentId
            let modelId = App.selection.activeModelId
            if (compId >= 0 && !QModelManager.query.hasComponent(compId))
                compId = -1
            if (compId >= 0) {
                // 组件有效时以组件为准同步所属模型
                modelId = QModelManager.query.findModelIdByComponent(compId)
            } else if (modelId >= 0 && !QModelManager.query.hasModel(modelId)) {
                modelId = -1
            }
            App.selection.activeComponentId = compId
            App.selection.activeModelId = modelId
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

                onClicked: objectTree.showAllNodes()
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

                onClicked: objectTree.hideAllNodes()
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
            readonly property bool _isActiveModel:
                viewDelegate.depth === 0
                && viewDelegate.model.nodeId === App.selection.activeModelId
            readonly property bool _isActiveComponent:
                viewDelegate.depth === 1
                && viewDelegate.model.nodeId === App.selection.activeComponentId

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
                    font.weight: viewDelegate._isActiveModel
                                 || viewDelegate._isActiveComponent
                                 ? Font.Bold : Font.Normal
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
                    visible: viewDelegate.depth <= 2
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
                    && viewDelegate.depth <= 2
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
                        else if (viewDelegate.depth === 1)
                            App.componentVisibilityUpdated(viewDelegate.model.nodeId, newVis, newVis)
                        else if (viewDelegate.depth === 2 && (viewDelegate.model.nodeType === objectTree._nodeTypeMesh
                            || viewDelegate.model.nodeType === objectTree._nodeTypeGeometry))
                            objectTree.emitComponentVisibility(viewDelegate.model.componentId)
                        return
                    }
                    if (mouse.button === Qt.RightButton) {
                        if (viewDelegate.depth > 2) return
                        contextMenu.popup()
                    } else {
                        let idx = treeModel.findIndexByNodeId(viewDelegate.model.nodeId, viewDelegate.depth)
                        let compId = viewDelegate.model.componentId

                        if (!idx.valid) {
                            return
                        } else if (viewDelegate.depth === 0
                                   && App.selection.activeModelId === viewDelegate.model.nodeId) {
                            // Model 与 Component 是同一业务目标，取消时同步清空。
                            viewDelegate.treeView.selectionModel.clear()
                            App.selection.activeComponentId = -1
                            App.selection.activeModelId = -1
                        } else if (viewDelegate.depth === 0) {
                            // 选择 Model 时同步关联其第一个 Component。
                            viewDelegate.treeView.selectionModel.setCurrentIndex(idx, ItemSelectionModel.ClearAndSelect)
                            App.selection.activeModelId = viewDelegate.model.nodeId
                            App.selection.activeComponentId = objectTree.firstComponentId(viewDelegate.model.nodeId)
                        } else if (viewDelegate.depth === 1
                                   && App.selection.activeComponentId === compId) {
                            // Component 与所属 Model 同步取消。
                            viewDelegate.treeView.selectionModel.clear()
                            App.selection.activeComponentId = -1
                            App.selection.activeModelId = -1
                        } else {
                            viewDelegate.treeView.selectionModel.setCurrentIndex(idx, ItemSelectionModel.ClearAndSelect)
                            App.selection.activeModelId = QModelManager.query.findModelIdByComponent(compId)
                            App.selection.activeComponentId = compId
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

                    onTriggered: objectTree.hideNode(
                        viewDelegate.model.nodeId, viewDelegate.depth,
                        viewDelegate.model.nodeType, viewDelegate.model.componentId)
                }

                MenuItem {
                    id: isolateItem
                    text: "隔离"
                    implicitHeight: 30

                    background: Rectangle {
                        color: isolateItem.hovered ? "#f0f0f0" : "transparent"
                    }

                    contentItem: Text {
                        text: isolateItem.text
                        color: isolateItem.hovered ? "#1976d2" : "#333333"
                        font.pixelSize: 12
                        font.family: "Microsoft YaHei"
                        leftPadding: 15
                        verticalAlignment: Text.AlignVCenter
                    }

                    onTriggered: {
                        if (viewDelegate.depth === 0)
                            objectTree.isolateModel(viewDelegate.model.nodeId)
                        else if (viewDelegate.depth === 1)
                            objectTree.isolateNode(viewDelegate.model.nodeId)
                        else if (viewDelegate.depth === 2)
                            objectTree.isolateSubNode(
                                viewDelegate.model.nodeId,
                                viewDelegate.model.nodeType,
                                viewDelegate.model.componentId)
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

                    onTriggered: objectTree.showNode(
                        viewDelegate.model.nodeId, viewDelegate.depth,
                        viewDelegate.model.nodeType, viewDelegate.model.componentId)
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
                        objectTree.deleteNode(
                            viewDelegate.model.nodeId, viewDelegate.depth,
                            viewDelegate.model.nodeType, viewDelegate.model.componentId)
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

    function emitComponentVisibility(componentId) {
        let compIdx = treeModel.findIndexByNodeId(componentId, 1)
        if (!compIdx || !compIdx.valid) return
        let meshVis = false, geomVis = false
        for (let i = 0; i < treeModel.rowCount(compIdx); i++) {
            let childIdx = treeModel.index(i, 0, compIdx)
            let cType = treeModel.data(childIdx, TreeModel.NodeTypeRole)
            let cVis = treeModel.data(childIdx, TreeModel.IsVisibleRole)
            if (cType === objectTree._nodeTypeMesh) meshVis = cVis
            else if (cType === objectTree._nodeTypeGeometry) geomVis = cVis
        }
        App.componentVisibilityUpdated(componentId, meshVis, geomVis)
    }

    function hideNode(nodeId, depth, nodeType, componentId) {
        let idx = treeModel.findIndexByNodeId(nodeId, depth)
        if (!idx || !idx.valid) return
        treeModel.setVisibility(idx, false)
        if (depth === 0)
            App.modelVisibilityUpdated(nodeId, false)
        else if (depth === 1)
            App.componentVisibilityUpdated(nodeId, false, false)
        else if (depth === 2 && (nodeType === objectTree._nodeTypeMesh || nodeType === objectTree._nodeTypeGeometry))
            emitComponentVisibility(componentId)
    }

    function showNode(nodeId, depth, nodeType, componentId) {
        let idx = treeModel.findIndexByNodeId(nodeId, depth)
        if (!idx || !idx.valid) return
        treeModel.setVisibility(idx, true)
        if (depth === 0)
            App.modelVisibilityUpdated(nodeId, true)
        else if (depth === 1)
            App.componentVisibilityUpdated(nodeId, true, true)
        else if (depth === 2 && (nodeType === objectTree._nodeTypeMesh || nodeType === objectTree._nodeTypeGeometry))
            emitComponentVisibility(componentId)
    }

    function deleteNode(nodeId, depth, nodeType, componentId) {
        treeView.selectionModel.clear()
        if (depth === 0)
            QModelManager.removeModel(nodeId)
        else if (depth === 1)
            QModelManager.removeComponent(nodeId)
        else if (depth === 2 && nodeType === objectTree._nodeTypeMesh)
            QModelManager.removeMesh(componentId)
        else if (depth === 2 && nodeType === objectTree._nodeTypeGeometry)
            QModelManager.removeGeometry(componentId)
    }

    function hideAllNodes() {
        treeModel.setAllVisibility(false)
        for (let i = 0; i < treeModel.rowCount(); i++) {
            let idx = treeModel.index(i, 0)
            App.modelVisibilityUpdated(
                treeModel.data(idx, TreeModel.NodeIdRole), false)
        }
    }

    function showAllNodes() {
        treeModel.setAllVisibility(true)
        for (let i = 0; i < treeModel.rowCount(); i++) {
            let idx = treeModel.index(i, 0)
            App.modelVisibilityUpdated(
                treeModel.data(idx, TreeModel.NodeIdRole), true)
        }
    }

    function isolateNode(nodeId) {
        let models = QModelManager.query.listModels()
        for (let i = 0; i < models.length; i++) {
            let mid = models[i].model_id
            let comps = QModelManager.query.getComponentsSummary(mid)
            for (let j = 0; j < comps.length; j++) {
                let cid = comps[j].component_id
                let visible = (cid === nodeId)
                let idx = treeModel.findIndexByNodeId(cid, 1)
                if (idx && idx.valid) {
                    treeModel.setVisibility(idx, visible)
                    App.componentVisibilityUpdated(cid, visible, visible)
                }
            }
        }
    }

    function isolateSubNode(nodeId, nodeType, componentId) {
        let models = QModelManager.query.listModels()
        for (let i = 0; i < models.length; i++) {
            let mid = models[i].model_id
            let comps = QModelManager.query.getComponentsSummary(mid)
            for (let j = 0; j < comps.length; j++) {
                let cid = comps[j].component_id
                let vis = (cid === componentId)
                let idx = treeModel.findIndexByNodeId(cid, 1)
                if (idx && idx.valid) {
                    treeModel.setVisibility(idx, vis)
                    App.componentVisibilityUpdated(cid, vis, vis)
                }
            }
        }

        let compIdx = treeModel.findIndexByNodeId(componentId, 1)
        if (compIdx && compIdx.valid) {
            let childCount = treeModel.rowCount(compIdx)
            for (let ci = 0; ci < childCount; ci++) {
                let childIdx = treeModel.index(ci, 0, compIdx)
                let childNodeId = treeModel.data(childIdx, TreeModel.NodeIdRole)
                let childNodeType = treeModel.data(childIdx, TreeModel.NodeTypeRole)
                if (childNodeId !== nodeId
                    && (childNodeType === objectTree._nodeTypeMesh
                        || childNodeType === objectTree._nodeTypeGeometry)) {
                    treeModel.setVisibility(childIdx, false)
                }
            }
            emitComponentVisibility(componentId)
        }
    }

    function isolateModel(modelId) {
        let models = QModelManager.query.listModels()
        for (let i = 0; i < models.length; i++) {
            let mid = models[i].model_id
            let visible = (mid === modelId)
            let idx = treeModel.findIndexByNodeId(mid, 0)
            if (idx && idx.valid) {
                treeModel.setVisibility(idx, visible)
                App.modelVisibilityUpdated(mid, visible)
            }
        }
    }

    function isolateSelection(ids) {
        let models = QModelManager.query.listModels()
        for (let i = 0; i < models.length; i++) {
            let mid = models[i].model_id
            let comps = QModelManager.query.getComponentsSummary(mid)
            for (let j = 0; j < comps.length; j++) {
                let cid = comps[j].component_id
                let visible = ids.includes(cid)
                let idx = treeModel.findIndexByNodeId(cid, 1)
                if (idx && idx.valid) {
                    treeModel.setVisibility(idx, visible)
                    App.componentVisibilityUpdated(cid, visible, visible)
                }
            }
        }
    }

    function reverseDisplayed() {
        let modelCount = treeModel.rowCount()
        for (let mi = 0; mi < modelCount; mi++) {
            let modelIdx = treeModel.index(mi, 0)
            let compCount = treeModel.rowCount(modelIdx)
            for (let ci = 0; ci < compCount; ci++) {
                let compIdx = treeModel.index(ci, 0, modelIdx)
                let cid = treeModel.data(compIdx, TreeModel.NodeIdRole)
                let isVis = treeModel.data(compIdx, TreeModel.IsVisibleRole)
                treeModel.setVisibility(compIdx, !isVis)
                App.componentVisibilityUpdated(cid, !isVis, !isVis)
            }
        }
    }

    Component.onCompleted: {
        App.registry.treeModel = treeModel
        App.registry.objectTree = objectTree
    }
}
