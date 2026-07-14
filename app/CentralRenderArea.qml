import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import app.core
import app.model
import app.render

Page {
    id: root
    background: null
    anchors.fill: parent

    footer: ToolBar {
        id: toolbar
        height: 25
        RowLayout {
            anchors.fill: parent
            ToolButton {
                text: "边渲染"
                checkable: true
                checked: myItem.cur_edge_render
                Layout.preferredWidth: 50
                Layout.fillHeight: true
                onClicked: {
                    myItem.setComponentEdgeRender(App.selection.activeComponentId, !myItem.cur_edge_render)
                }
            }
            ToolButton {
                text: "块渲染"
                checkable: true
                Layout.preferredWidth: 50
                Layout.fillHeight: true
                onClicked: {
                    if (checked) {
                        myItem.setRenderMode(App.selection.activeModelId, "Block")
                    } else {
                        myItem.setRenderMode(App.selection.activeModelId, "Face")
                    }
                }
            }
            ToolButton {
                text: "裁剪"
                checkable: true
                Layout.preferredWidth: 50
                Layout.fillHeight: true
                onClicked: {
                    myItem.setMeshClip(checked)
                }
            }
            ToolButton {
                text: "重置视图"
                Layout.preferredWidth: 70
                Layout.fillHeight: true
                onClicked: myItem.resetCamera()
            }
            Label {
                Layout.fillWidth: true
            }
        }
    }

    Page {
        id: renderWindowPage
        anchors.fill: parent
        background: null

        Rectangle {
            id: borderRectangle
            anchors.fill: parent
            border.color: "black"
            border.width: 3
            color: "transparent"
            z: 1
        }

        QRenderWindow {
            id: myItem
            anchors.fill: parent
            anchors.margins: 3
            query: QModelManager.query

            function doHideSelected() {
                let compId = App.selection.activeComponentId
                let tm = App.registry.treeModel
                if (tm && compId >= 0) {
                    let idx = tm.findIndexByNodeId(compId, 1)
                    if (idx && idx.valid)
                        tm.setVisibility(idx, false)
                }
                myItem.hideSelected()
            }

            function doIsolateSelected() {
                let compId = App.selection.activeComponentId
                let models = QModelManager.query.listModels()
                let tm = App.registry.treeModel
                if (tm) {
                    for (let i = 0; i < models.length; i++) {
                        let mid = models[i].model_id
                        let comps = QModelManager.query.getComponentsSummary(mid)
                        for (let j = 0; j < comps.length; j++) {
                            let cid = comps[j].component_id
                            let visible = (cid === compId)
                            let idx = tm.findIndexByNodeId(cid, 1)
                            if (idx && idx.valid)
                                tm.setVisibility(idx, visible)
                        }
                    }
                }
                myItem.setIsolateComponent()
            }

            function doShowSelected() {
                let compId = App.selection.activeComponentId
                let tm = App.registry.treeModel
                if (tm && compId >= 0) {
                    let idx = tm.findIndexByNodeId(compId, 1)
                    if (idx && idx.valid)
                        tm.setVisibility(idx, true)
                }
                myItem.showSelected()
            }

            function doHideAll() {
                let tm = App.registry.treeModel
                if (tm) {
                    let modelCount = tm.rowCount()
                    for (let mi = 0; mi < modelCount; mi++)
                        tm.setVisibility(tm.index(mi, 0), false)
                }
                myItem.hideAll()
            }

            function doShowAll() {
                let tm = App.registry.treeModel
                if (tm) {
                    let modelCount = tm.rowCount()
                    for (let mi = 0; mi < modelCount; mi++)
                        tm.setVisibility(tm.index(mi, 0), true)
                }
                myItem.showAll()
            }

            function doReverseDisplayed() {
                let tm = App.registry.treeModel
                if (tm) {
                    let modelCount = tm.rowCount()
                    for (let mi = 0; mi < modelCount; mi++) {
                        let modelIdx = tm.index(mi, 0)
                        let compCount = tm.rowCount(modelIdx)
                        for (let ci = 0; ci < compCount; ci++) {
                            let compIdx = tm.index(ci, 0, modelIdx)
                            let isVis = tm.data(compIdx, TreeModel.IsVisibleRole)
                            tm.setVisibility(compIdx, !isVis)
                        }
                    }
                }
                myItem.reverseDisplayed()
            }

            onRightClicked: {
                viewportMenu.popup()
            }

            Connections {
                target: QModelManager.observer
                function onModelAdded(model_id) { myItem.onModelChanged(model_id) }
                function onModelChanged(model_id) { myItem.onModelChanged(model_id) }
                function onModelRemoved(model_id) { myItem.deleteModel(model_id) }
                function onComponentChanged(component_id) { myItem.onComponentChanged(component_id) }
                function onComponentRemoved(component_id) { myItem.deleteComponent(component_id) }
            }

            Connections {
                target: App.selection
                function onActiveComponentIdChanged() {
                    if (App.selection.activeComponentId >= 0)
                        myItem.setSelectComponent(App.selection.activeComponentId)
                    else
                        myItem.clearSelection()
                }
                function onSelectModeChanged() { myItem.setSelectMode(App.selection.selectMode) }
            }

            Connections {
                target: App
                function onModelVisibilityUpdated(modelId, visible) { myItem.setVisibility(modelId, visible) }
                function onComponentVisibilityUpdated(componentId, visible) { myItem.setComponentVisibility(componentId, visible) }
            }
        }

        Selector {
            id: selector
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.topMargin: 10
            anchors.leftMargin: 10

            onClearButtonClicked: {
                myItem.clearSelection()
            }

            onConfirmButtonClicked: {
                selector.selection = myItem.selectedIDs
            }
        }

        Menu {
            id: viewportMenu

            implicitWidth: 140
            width: implicitWidth
            height: implicitHeight

            padding: 0
            topPadding: 0
            bottomPadding: 0
            leftPadding: 0
            rightPadding: 0

            property int textLeftInset: 18
            property int textRightInset: 12

            background: Rectangle {
                anchors.fill: parent
                color: "#ffffff"
                border.color: "#d0d0d0"
                border.width: 1
                radius: 4
            }

            component StyledMenuItem: MenuItem {
                id: control
                property bool shown: true

                visible: shown
                enabled: shown

                implicitHeight: shown ? 30 : 0
                height: implicitHeight

                width: viewportMenu.width
                implicitWidth: viewportMenu.width

                background: Rectangle {
                    anchors.fill: parent
                    color: control.hovered ? "#f0f0f0" : "transparent"
                }

                contentItem: Text {
                    anchors.fill: parent
                    anchors.leftMargin: viewportMenu.textLeftInset
                    anchors.rightMargin: viewportMenu.textRightInset

                    text: control.text
                    color: control.hovered ? "#1976d2" : "#333333"
                    font.pixelSize: 12
                    font.family: "Microsoft YaHei"
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignLeft
                    elide: Text.ElideRight
                }
            }

            component StyledSeparator: MenuSeparator {
                width: viewportMenu.width
                implicitWidth: viewportMenu.width
                implicitHeight: 6
                height: visible ? implicitHeight : 0
            }

            StyledMenuItem {
                text: "隐藏"
                shown: App.selection.activeComponentId >= 0
                onTriggered: myItem.doHideSelected()
            }

            StyledMenuItem {
                text: "隔离"
                shown: App.selection.activeComponentId >= 0
                onTriggered: myItem.doIsolateSelected()
            }

            StyledMenuItem {
                text: "显示"
                shown: App.selection.activeComponentId >= 0
                onTriggered: myItem.doShowSelected()
            }

            StyledSeparator {
                visible: App.selection.activeComponentId >= 0
            }

            StyledMenuItem {
                text: "全部隐藏"
                shown: App.selection.activeComponentId < 0
                onTriggered: myItem.doHideAll()
            }

            StyledMenuItem {
                text: "全部显示"
                shown: App.selection.activeComponentId < 0
                onTriggered: myItem.doShowAll()
            }

            StyledMenuItem {
                text: "反转显示"
                shown: true
                onTriggered: myItem.doReverseDisplayed()
            }
        }
    }

    Component.onCompleted: {
        App.registry.renderWindow = myItem
        Qt.callLater(function() { myItem.resetCamera() })
    }
}
