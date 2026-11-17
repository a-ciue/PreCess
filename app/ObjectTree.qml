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

    //! @brief 自身菜单打开期间不响应外部左键清空。
    property bool _menuOpen: false

    // 加粗行：仅活动 Model/Component；更深层级只高亮，不加粗。
    function isNodeEmphasized(nodeId, depth) {
        if (depth === 0)
            return nodeId === App.selection.activeModelId
        if (depth === 1)
            return nodeId === App.selection.activeComponentId
        return false
    }

    // 清空行选择，保留导航位置与活动对象。
    function clearRowSelection() {
        treeView.selectionModel.clearSelection()
    }

    // 清空行选择与导航位置（undo/redo 等全量重置）。
    function resetSelection() {
        treeView.selectionModel.clear()
    }

    function clearActiveTarget() {
        App.selection.activeComponentId = -1
        App.selection.activeModelId = -1
    }

    function isActiveNode(nodeId, depth) {
        if (depth === 0)
            return App.selection.activeModelId >= 0 && nodeId === App.selection.activeModelId
        if (depth === 1)
            return App.selection.activeComponentId >= 0 && nodeId === App.selection.activeComponentId
        return false
    }

    // 设置活动组件：仅 Model/Component 可切换，且不能重复设置当前活动行。
    function makeNodeCurrent(nodeId, depth, componentId) {
        if (depth === 0) {
            if (isActiveNode(nodeId, depth))
                return
            App.selection.activeModelId = nodeId
            App.selection.activeComponentId = firstComponentId(nodeId)
        } else if (depth === 1) {
            if (isActiveNode(nodeId, depth))
                return
            App.selection.activeComponentId = componentId
            App.selection.activeModelId = QModelManager.query.findModelIdByComponent(componentId)
        }
    }

    // 左键选择入口：单行替换选中；显式左键再次点击已选中行时取消。
    function selectRow(index, toggle = false) {
        if (!index || !index.valid)
            return
        treeView.forceActiveFocus()
        if (toggle && treeView.selectionModel.isSelected(index)) {
            clearRowSelection()
            return
        }
        treeView.selectionModel.setCurrentIndex(index, ItemSelectionModel.ClearAndSelect)
    }

    // 单目标命令读取当前选中行。
    function selectedTarget() {
        const indexes = treeView.selectionModel.selectedIndexes
        if (indexes.length !== 1)
            return null
        const index = indexes[0]
        return {
            nodeId: treeModel.data(index, TreeModel.NodeIdRole),
            depth: objectTree.nodeDepth(index),
            nodeType: treeModel.data(index, TreeModel.NodeTypeRole),
            componentId: treeModel.data(index, TreeModel.ComponentIdRole)
        }
    }

    function hideSelection() {
        const target = objectTree.selectedTarget()
        if (target)
            hideNode(target.nodeId, target.depth, target.nodeType, target.componentId)
    }

    function showSelection() {
        const target = objectTree.selectedTarget()
        if (target)
            showNode(target.nodeId, target.depth, target.nodeType, target.componentId)
    }

    function isolateSelectedNode() {
        const target = objectTree.selectedTarget()
        if (!target)
            return
        if (target.depth === 0)
            isolateModel(target.nodeId)
        else if (target.depth === 1)
            isolateNode(target.nodeId)
        else if (target.depth === 2)
            isolateSubNode(target.nodeId, target.nodeType, target.componentId)
    }

    function containsGlobalPoint(x, y) {
        if (!objectTree.visible)
            return false
        return objectTree.contains(objectTree.mapFromGlobal(x, y))
    }

    // 应用级左键按下：点击对象树外部时清空行选择，活动对象保持不变。
    function handleGlobalLeftPress(x, y) {
        if (_menuOpen)
            return
        if (!objectTree.containsGlobalPoint(x, y))
            objectTree.clearRowSelection()
    }

    function notifyMenuClosed() {
        _menuOpen = false
    }

    // 树内容底部在视口中的位置，用于空白右键区跟随滚动。
    readonly property real _blankTop:
        Math.max(0, Math.min(treeView.height, treeView.contentHeight - treeView.contentY))

    // 全部展开/折叠当前节点及其整个子树，逐级递归到底。
    function expandSubtree(row) {
        treeView.expandRecursively(row, -1)
        treeView.forceLayout()
    }

    function collapseSubtree(row) {
        treeView.collapseRecursively(row)
        treeView.forceLayout()
    }

    Connections {
        target: QModelManager.observer

        function onModelAdded(modelId) {
            treeModel.requestRefresh()
        }

        function onModelRemoved(modelId) {
            if (App.selection.activeModelId === modelId)
                objectTree.clearActiveTarget()
            treeModel.requestRefresh()
        }

        function onModelChanged(modelId) {
            treeModel.requestRefresh()
        }

        function onComponentRemoved(componentId) {
            // 活动对象失效时清理；行选择由树结构更新负责。
            if (App.selection.activeComponentId === componentId)
                objectTree.clearActiveTarget()
            treeModel.requestRefresh()
        }

        function onComponentChanged(componentId) {
            treeModel.requestRefresh()
        }
    }

    Connections {
        target: QModelManager.undoStack

        function onApplied() {
            objectTree.resetSelection()
            const activeModelId = App.selection.activeModelId
            const activeComponentId = App.selection.activeComponentId
            if (activeModelId >= 0 && !QModelManager.query.hasModel(activeModelId)) {
                objectTree.clearActiveTarget()
            } else if (activeComponentId >= 0
                       && !QModelManager.query.hasComponent(activeComponentId)) {
                App.selection.activeComponentId = -1
            }
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
        // 键盘导航由下方 Keys 统一处理，保持单选语义。
        keyNavigationEnabled: false
        selectionMode: TableView.SingleSelection

        property int toggleExpandRow: -1
        onExpanded: (row, depth) => toggleExpandRow = row
        onCollapsed: (row, recursively) => toggleExpandRow = row

        delegate: TreeViewDelegate {
            id: viewDelegate
            height: _rowHeight

            readonly property real _padding: 6
            readonly property real _rowHeight: 22
            readonly property real _indentWidth: 20
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
                border.width: viewDelegate.selected ? 1 : 0
                border.color: Theme.primary

                // 导航 current 与 selection 分离：当前行用左侧强调条标记。
                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 2
                    color: Theme.primary
                    visible: viewDelegate.current && treeView.activeFocus
                }
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
                    font.weight: objectTree.isNodeEmphasized(viewDelegate.model.nodeId, viewDelegate.depth)
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
                    const index = treeView.index(viewDelegate.row, 0)
                    if (mouse.button === Qt.RightButton) {
                        // 右键只读取当前选择，不改变选中；未选中行使用全局菜单。
                        if (!viewDelegate.selected) {
                            blankContextMenu.popup()
                        } else if (viewDelegate.depth <= 1) {
                            nodeContextMenu.popup()
                        } else if (viewDelegate.depth === 2) {
                            nodeActionsMenu.popup()
                        } else {
                            deepContextMenu.popup()
                        }
                        return
                    }
                    objectTree.selectRow(index, true)
                }
            }

            AppMenu {
                id: nodeContextMenu
                objectName: "nodeContextMenu"

                onOpened: objectTree._menuOpen = true
                onClosed: objectTree.notifyMenuClosed()

                MenuItem {
                    objectName: "activeComponentItem"
                    text: objectTree.isActiveNode(viewDelegate.model.nodeId, viewDelegate.depth)
                          ? "取消活动组件" : "设置活动组件"
                    onTriggered: {
                        if (objectTree.isActiveNode(viewDelegate.model.nodeId, viewDelegate.depth)) {
                            objectTree.clearActiveTarget()
                        } else {
                            objectTree.makeNodeCurrent(
                                viewDelegate.model.nodeId, viewDelegate.depth,
                                viewDelegate.model.componentId)
                        }
                    }
                }

                MenuSeparator {}

                MenuItem {
                    text: "隐藏"
                    onTriggered: objectTree.hideSelection()
                }

                MenuItem {
                    text: "隔离"
                    onTriggered: objectTree.isolateSelectedNode()
                }

                MenuItem {
                    objectName: "showNode-" + viewDelegate.model.nodeType + "-"
                                + (viewDelegate.depth === 0 ? viewDelegate.model.nodeId : viewDelegate.model.componentId)
                    text: "显示"
                    onTriggered: objectTree.showSelection()
                }

                MenuSeparator {}

                MenuItem {
                    text: "展开"
                    onTriggered: objectTree.expandSubtree(viewDelegate.row)
                }

                MenuItem {
                    text: "折叠"
                    onTriggered: objectTree.collapseSubtree(viewDelegate.row)
                }

                MenuSeparator {}

                // 破坏性操作：文字与悬停底色用危险色，与其余菜单项区分
                MenuItem {
                    id: deleteActiveItem
                    text: "删除"

                    contentItem: Text {
                        text: deleteActiveItem.text
                        color: Theme.danger
                        font.pixelSize: Theme.fontSizeCaption
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: deleteActiveItem.padding
                    }

                    background: Rectangle {
                        color: deleteActiveItem.hovered ? Theme.dangerTint : "transparent"
                    }

                    onTriggered: {
                        nodeContextMenu.close()
                        objectTree.deleteSelectedNode()
                    }
                }
            }

            AppMenu {
                id: nodeActionsMenu
                objectName: "nodeActionsMenu"

                onOpened: objectTree._menuOpen = true
                onClosed: objectTree.notifyMenuClosed()

                MenuItem {
                    text: "隐藏"
                    onTriggered: objectTree.hideSelection()
                }

                MenuItem {
                    text: "隔离"
                    onTriggered: objectTree.isolateSelectedNode()
                }

                MenuItem {
                    objectName: "showNode-" + viewDelegate.model.nodeType + "-"
                                + (viewDelegate.depth === 0 ? viewDelegate.model.nodeId : viewDelegate.model.componentId)
                    text: "显示"
                    onTriggered: objectTree.showSelection()
                }

                MenuSeparator {}

                MenuItem {
                    text: "展开"
                    onTriggered: objectTree.expandSubtree(viewDelegate.row)
                }

                MenuItem {
                    text: "折叠"
                    onTriggered: objectTree.collapseSubtree(viewDelegate.row)
                }

                MenuSeparator {}

                MenuItem {
                    id: deleteActionsItem
                    text: "删除"

                    contentItem: Text {
                        text: deleteActionsItem.text
                        color: Theme.danger
                        font.pixelSize: Theme.fontSizeCaption
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: deleteActionsItem.padding
                    }

                    background: Rectangle {
                        color: deleteActionsItem.hovered ? Theme.dangerTint : "transparent"
                    }

                    onTriggered: {
                        nodeActionsMenu.close()
                        objectTree.deleteSelectedNode()
                    }
                }
            }

            AppMenu {
                id: deepContextMenu
                objectName: "deepContextMenu"

                onOpened: objectTree._menuOpen = true
                onClosed: objectTree.notifyMenuClosed()

                MenuItem {
                    text: "功能开发中"
                    enabled: false
                }
            }
        }

        selectionModel: ItemSelectionModel {}

        Keys.onPressed: (event) => {
            if (event.modifiers & (Qt.AltModifier | Qt.MetaModifier))
                return
            const rows = treeView.rows
            if (rows <= 0)
                return
            const currentIndex = treeView.selectionModel.currentIndex
            let row = currentIndex && currentIndex.valid
                    ? treeView.rowAtIndex(currentIndex) : -1
            if (row < 0)
                row = 0

            if (event.key === Qt.Key_Left || event.key === Qt.Key_Right) {
                if (event.key === Qt.Key_Left) {
                    if (treeView.isExpanded(row)) {
                        treeView.collapseRecursively(row)
                    } else {
                        const parent = treeView.index(row, 0).parent
                        const parentRow = parent && parent.valid
                                ? treeView.rowAtIndex(parent) : -1
                        if (parentRow >= 0)
                            objectTree.selectRow(treeView.index(parentRow, 0))
                    }
                } else if (!treeView.isExpanded(row)) {
                    treeView.expand(row)
                }
                event.accepted = true
                return
            }

            let targetRow = -1
            switch (event.key) {
            case Qt.Key_Up: targetRow = row - 1; break
            case Qt.Key_Down: targetRow = row + 1; break
            case Qt.Key_PageUp: targetRow = row - 10; break
            case Qt.Key_PageDown: targetRow = row + 10; break
            case Qt.Key_Home: targetRow = 0; break
            case Qt.Key_End: targetRow = rows - 1; break
            default: return
            }
            targetRow = Math.max(0, Math.min(rows - 1, targetRow))
            objectTree.selectRow(treeView.index(targetRow, 0))
            event.accepted = true
        }

        Keys.onDeletePressed: {
            if (objectTree.deleteSelectedNode())
                event.accepted = true
        }

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

    // 空白区：左键清除选中，右键提供全局展开/折叠。
    MouseArea {
        anchors.left: treeView.left
        anchors.right: treeView.right
        y: treeView.y + objectTree._blankTop
        height: treeView.height - objectTree._blankTop
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton)
                blankContextMenu.popup()
            else
                objectTree.clearRowSelection()
        }
    }

    AppMenu {
        id: blankContextMenu
        objectName: "blankContextMenu"

        onOpened: objectTree._menuOpen = true
        onClosed: objectTree.notifyMenuClosed()

        MenuItem {
            text: "全部展开"
            onTriggered: {
                objectTree.expandSubtree(-1)
            }
        }

        MenuItem {
            text: "全部折叠"
            onTriggered: {
                objectTree.collapseSubtree(-1)
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

    // 模型索引深度：根节点为 0，逐级向下递增。
    function nodeDepth(index) {
        let depth = 0
        let parent = index.parent
        while (parent && parent.valid) {
            ++depth
            parent = parent.parent
        }
        return depth
    }

    // Delete 快捷键入口：仅取得实际选中行并调用既有删除入口。
    function deleteSelectedNode() {
        const indexes = treeView.selectionModel.selectedIndexes
        if (indexes.length === 0)
            return false
        const idx = indexes[0]
        const depth = nodeDepth(idx)
        if (depth < 0 || depth > 2)
            return false
        const nodeType = treeModel.data(idx, TreeModel.NodeTypeRole)
        const canDelete = (depth === 0 && nodeType === _nodeTypeModel)
                || (depth === 1 && nodeType === _nodeTypeComponent)
                || (depth === 2
                    && (nodeType === _nodeTypeMesh || nodeType === _nodeTypeGeometry))
        if (!canDelete)
            return false
        const componentId = treeModel.data(idx, TreeModel.ComponentIdRole)
        const nodeId = treeModel.data(idx, TreeModel.NodeIdRole)
        deleteNode(nodeId, depth, nodeType, componentId)
        return true
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
