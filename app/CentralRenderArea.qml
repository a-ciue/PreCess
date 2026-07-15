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
    }

    Component.onCompleted: {
        App.registry.renderWindow = myItem
        Qt.callLater(function() { myItem.resetCamera() })
    }
}
