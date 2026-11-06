/**
 * @file ObjectTree.qml
 * @brief 对象树面板：模型/组件/网格与几何节点的展示、显隐与右键管理
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import app.core
import app.model

Pane {
    id: objectTree
    padding: 0
    clip: true
    // 背景由停靠面板的统一面底板承载（Fusion 默认 Pane 背景为不透明 palette.window）
    background: null

    TreeModel {
        id: treeModel
        modelQuery: QModelManager.query
        onVisibilityRequested: (nodeType, objectId, visible) => {
            const viewport = App.registry.renderWindow
            if (nodeType === objectTree._nodeTypeModel)
                viewport.setVisibility(objectId, visible)
            else if (nodeType === objectTree._nodeTypeComponent)
                viewport.setComponentVisibility(objectId, visible)
            else if (nodeType === objectTree._nodeTypeMesh)
                viewport.setMeshVisibility(objectId, visible)
            else if (nodeType === objectTree._nodeTypeGeometry)
                viewport.setGeometryVisibility(objectId, visible)
        }
    }

    // 选择 Model 时默认关联其第一个 Component，二者共同组成当前操作目标。
    function firstComponentId(modelId) {
        let components = QModelManager.query.getComponentsSummary(modelId)
        return components.length > 0 ? components[0].component_id : -1
    }

    readonly property int _nodeTypeModel: 0
    readonly property int _nodeTypeComponent: 1
    readonly property int _nodeTypeMesh: 2
    readonly property int _nodeTypeGeometry: 3

    readonly property int _expansionComp: 1
    readonly property int _expansionGeoAndEle: 2
    readonly property int _expansionAll: 3

    // 取当前可见行中已展开节点的最大层级，用于层级按钮的再次点击折叠。
    function currentExpansionLevel() {
        let level = 0
        for (let row = 0; row < treeView.rows; ++row) {
            if (treeView.isExpanded(row))
                level = Math.max(level, treeView.depth(row) + 1)
        }
        return level
    }

    // 先统一折叠再展开到目标层级，确保深于目标的节点也被收起。
    function applyExpansionLevel(level) {
        treeView.collapseRecursively(-1)
        if (level > 0)
            treeView.expandRecursively(-1, level)
        treeView.forceLayout()
    }

    function toggleExpansionLevel(level) {
        applyExpansionLevel(currentExpansionLevel() >= level ? level - 1 : level)
    }

    Connections {
        target: QModelManager.observer

        function onModelAdded(modelId) {
            treeModel.requestRefresh()
        }

        function onModelRemoved(modelId) {
            if (App.selection.activeModelId === modelId) {
                treeView.selectionModel.clear()
                App.selection.activeComponentId = -1
                App.selection.activeModelId = -1
            }
            treeModel.requestRefresh()
        }

        function onModelChanged(modelId) {
            treeModel.requestRefresh()
        }

        function onComponentRemoved(componentId) {
            if (App.selection.activeComponentId === componentId) {
                // 实际删除当前组件后保留所属模型，并选择剩余的第一个组件。
                const ownerModelId = App.selection.activeModelId
                treeView.selectionModel.clear()
                App.selection.activeComponentId = -1
                treeModel.requestRefresh()
                App.selection.activeComponentId = QModelManager.query.hasModel(ownerModelId)
                        ? objectTree.firstComponentId(ownerModelId) : -1
            } else {
                treeModel.requestRefresh()
            }
        }

        function onComponentChanged(componentId) {
            treeModel.requestRefresh()
        }
    }

    Rectangle {
        id: headerBar
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 30
        color: Theme.surfaceAlt

        // 表头与内容区的细线分隔
        Rectangle {
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            height: 1
            color: Theme.border
        }

        Row {
            anchors { left: parent.left; leftMargin: 6; verticalCenter: parent.verticalCenter }
            spacing: 2

            ToolButton {
                objectName: "objectTreeToggleComp"
                text: "一级"
                implicitHeight: 26
                display: ToolButton.TextOnly
                flat: true
                font.pixelSize: Theme.fontSizeSmall
                Accessible.name: qsTr("展开或折叠到一级（组件）")

                ToolTip.visible: hovered
                ToolTip.text: qsTr("展开或折叠到一级（组件）")
                ToolTip.delay: 500

                onClicked: objectTree.toggleExpansionLevel(objectTree._expansionComp)
            }

            ToolButton {
                objectName: "objectTreeToggleGeo"
                text: "二级"
                implicitHeight: 26
                display: ToolButton.TextOnly
                flat: true
                font.pixelSize: Theme.fontSizeSmall
                Accessible.name: qsTr("展开或折叠到二级（几何与网格）")

                ToolTip.visible: hovered
                ToolTip.text: qsTr("展开或折叠到二级（几何与网格）")
                ToolTip.delay: 500

                onClicked: objectTree.toggleExpansionLevel(objectTree._expansionGeoAndEle)
            }

            ToolButton {
                objectName: "objectTreeToggleAll"
                text: "三级"
                implicitHeight: 26
                display: ToolButton.TextOnly
                flat: true
                font.pixelSize: Theme.fontSizeSmall
                Accessible.name: qsTr("展开或折叠到三级（元素）")

                ToolTip.visible: hovered
                ToolTip.text: qsTr("展开或折叠到三级（元素）")
                ToolTip.delay: 500

                onClicked: objectTree.toggleExpansionLevel(objectTree._expansionAll)
            }
        }

        Row {
            anchors { right: parent.right; rightMargin: 6; verticalCenter: parent.verticalCenter }
            spacing: 2

            ToolButton {
                icon.source: "qrc:/images/toolbar/Viewport/show-all.svg"
                icon.width: 20
                icon.height: 20
                implicitWidth: 26
                implicitHeight: 26
                display: ToolButton.IconOnly
                flat: true
                Accessible.name: qsTr("显示全部")

                ToolTip.visible: hovered
                ToolTip.text: qsTr("显示全部")
                ToolTip.delay: 500

                onClicked: objectTree.showAllNodes()
                }

            ToolButton {
                icon.source: "qrc:/images/toolbar/Viewport/hide-all.svg"
                icon.width: 20
                icon.height: 20
                implicitWidth: 26
                implicitHeight: 26
                display: ToolButton.IconOnly
                flat: true
                Accessible.name: qsTr("隐藏全部")

                ToolTip.visible: hovered
                ToolTip.text: qsTr("隐藏全部")
                ToolTip.delay: 500

                onClicked: objectTree.hideAllNodes()
                }
            }
        }

    TreeView {
        id: treeView
        objectName: "objectTreeView"
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

            readonly property real _padding: 6
            readonly property real _rowHeight: 22
            readonly property real _indentWidth: 20
            readonly property bool _isActiveModel:
                viewDelegate.depth === 0
                && viewDelegate.model.nodeId === App.selection.activeModelId
            readonly property bool _isActiveComponent:
                viewDelegate.depth === 1
                && viewDelegate.model.nodeId === App.selection.activeComponentId
            leftPadding: _padding + depth * _indentWidth + 18
            rightPadding: 0

            TableView.onPooled: indicatorAnim.complete()
            TableView.onReused: {
                if (treeView.toggleExpandRow === viewDelegate.row) {
                    treeView.toggleExpandRow = -1
                    indicatorAnim.start()
                }
            }

            background: Rectangle {
                anchors.fill: parent
                color: viewDelegate.selected ? Theme.primaryTint
                     : viewDelegate.hovered ? Theme.hoverOverlay
                     : "transparent"
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
                    color: viewDelegate.model.isVisible ? Theme.textSecondary : Theme.textDisabled
                    font.pixelSize: 10
                }
            }

            contentItem: RowLayout {
                spacing: 4

                Text {
                    id: nameText
                    Layout.fillWidth: true
                    Layout.minimumWidth: 50
                    text: viewDelegate.model.name || "N/A"
                    color: viewDelegate.model.isVisible ? Theme.textPrimary : Theme.textDisabled
                    font.pixelSize: Theme.fontSizeBody
                    font.family: Theme.monoFamily
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
                    color: viewDelegate.model.isVisible ? Theme.textSecondary : Theme.textDisabled
                    font.pixelSize: Theme.fontSizeSmall
                }

                Item {
                    Layout.fillWidth: true
                }

                Image {
                    id: eyeIcon
                    visible: viewDelegate.model.canToggleVisibility
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                    source: viewDelegate.model.isVisible
                        ? "qrc:/images/modeltree/show.svg"
                        : "qrc:/images/modeltree/hide.svg"
                    opacity: treeMouseArea._overEye ? 1.0 : 0.4

                    // 部分显示同时提供形状和文字提示，避免只靠颜色区分。
                    Text {
                        anchors.centerIn: parent
                        visible: viewDelegate.model.visibilityState === Qt.PartiallyChecked
                        text: "−"
                        color: Theme.textPrimary
                        font.bold: true
                    }

                    ToolTip.visible: treeMouseArea._overEye
                    ToolTip.text: viewDelegate.model.visibilityState === Qt.PartiallyChecked
                        ? qsTr("部分显示，点击显示全部")
                        : viewDelegate.model.isVisible ? qsTr("隐藏") : qsTr("显示")
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
                    && viewDelegate.model.canToggleVisibility
                    && mouseX > treeMouseArea.width - 22

                cursorShape: treeMouseArea._overEye ? Qt.PointingHandCursor : Qt.ArrowCursor

                onClicked: (mouse) => {
                    if (treeMouseArea._overEye) {
                        if (mouse.button === Qt.RightButton) return
                        let idx = treeModel.findIndexByNodeId(viewDelegate.model.nodeId, viewDelegate.depth)
                        let newVis = viewDelegate.model.visibilityState !== Qt.Checked
                        treeModel.requestVisibility(idx, newVis)
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
                            App.selection.activeComponentId = compId
                            App.selection.activeModelId = QModelManager.query.findModelIdByComponent(compId)
                        }
                    }
                }
            }

            AppMenu {
                id: contextMenu

                MenuItem {
                    text: "隐藏"
                    onTriggered: objectTree.hideNode(
                        viewDelegate.model.nodeId, viewDelegate.depth,
                        viewDelegate.model.nodeType, viewDelegate.model.componentId)
                }

                MenuItem {
                    text: "隔离"
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
                    objectName: "showNode-" + viewDelegate.model.nodeType + "-"
                                + (viewDelegate.depth === 0 ? viewDelegate.model.nodeId : viewDelegate.model.componentId)
                    text: "显示"
                    onTriggered: objectTree.showNode(
                        viewDelegate.model.nodeId, viewDelegate.depth,
                        viewDelegate.model.nodeType, viewDelegate.model.componentId)
                }

                MenuSeparator {}

                // 破坏性操作：文字与悬停底色用危险色，与其余菜单项区分
                MenuItem {
                    id: deleteItem
                    text: "删除"

                    contentItem: Text {
                        text: deleteItem.text
                        color: Theme.danger
                        font.pixelSize: Theme.fontSizeCaption
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: deleteItem.padding
                    }

                    background: Rectangle {
                        color: deleteItem.hovered ? Theme.dangerTint : "transparent"
                    }

                    onTriggered: {
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
                color: parent.hovered ? Theme.scrollBarHover : Theme.scrollBarIdle
            }
        }
    }

    // 子节点只更新对应对象，避免几何显隐触发网格显示时的面恢复。
    function hideNode(nodeId, depth, nodeType, componentId) {
        const idx = treeModel.findIndexByNodeId(nodeId, depth)
        treeModel.requestVisibility(idx, false)
    }

    // 显示范围和隐藏面恢复由管线统一决定，对象树只发送请求。
    function showNode(nodeId, depth, nodeType, componentId) {
        const idx = treeModel.findIndexByNodeId(nodeId, depth)
        treeModel.requestVisibility(idx, true)
    }

    function deleteNode(nodeId, depth, nodeType, componentId) {
        if (depth === 0) {
            QModelManager.removeModel(nodeId)
        } else if (depth === 1) {
            QModelManager.removeComponent(nodeId)
        } else if (depth === 2 && nodeType === objectTree._nodeTypeMesh) {
            QModelManager.removeMesh(componentId)
        } else if (depth === 2 && nodeType === objectTree._nodeTypeGeometry) {
            QModelManager.removeGeometry(componentId)
        }
    }

    function hideAllNodes() {
        treeModel.requestAllVisibility(false)
    }

    function showAllNodes() {
        // 与全部隐藏共用请求-回传路径，由管线逐组件恢复并回传最终状态。
        treeModel.requestAllVisibility(true)
    }

    function isolateNode(nodeId) {
        const models = QModelManager.query.listModels()
        for (const model of models) {
            const components = QModelManager.query.getComponentsSummary(model.model_id)
            for (const component of components) {
                const idx = treeModel.findIndexByNodeId(component.component_id, 1)
                if (!idx || !idx.valid) continue
                treeModel.requestVisibility(idx, component.component_id === nodeId)
            }
        }
    }

    function isolateSubNode(nodeId, nodeType, componentId) {
        const models = QModelManager.query.listModels()
        for (const model of models) {
            const components = QModelManager.query.getComponentsSummary(model.model_id)
            for (const component of components) {
                const idx = treeModel.findIndexByNodeId(component.component_id, 1)
                if (!idx || !idx.valid) continue
                if (component.component_id !== componentId) {
                    treeModel.requestVisibility(idx, false)
                    continue
                }
                // 目标组件直接提交最终子节点开关，不先显示整组件再二次隐藏。
                for (let row = 0; row < treeModel.rowCount(idx); ++row) {
                    const child = treeModel.index(row, 0, idx)
                    treeModel.requestVisibility(child, treeModel.data(child, TreeModel.NodeTypeRole) === nodeType)
                }
            }
        }
    }

    function isolateModel(modelId) {
        const models = QModelManager.query.listModels()
        for (const model of models) {
            const idx = treeModel.findIndexByNodeId(model.model_id, 0)
            if (!idx || !idx.valid) continue
            treeModel.requestVisibility(idx, model.model_id === modelId)
        }
    }

    function isolateSelection(ids) {
        const models = QModelManager.query.listModels()
        for (const model of models) {
            const components = QModelManager.query.getComponentsSummary(model.model_id)
            for (const component of components) {
                const idx = treeModel.findIndexByNodeId(component.component_id, 1)
                if (!idx || !idx.valid) continue
                treeModel.requestVisibility(idx, ids.includes(component.component_id))
            }
        }
    }

    function reverseDisplayed() {
        App.registry.renderWindow.reverseSceneVisibility()
    }

    Component.onCompleted: {
        App.registry.treeModel = treeModel
        App.registry.objectTree = objectTree
    }
}
