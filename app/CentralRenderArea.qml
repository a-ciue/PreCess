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

    // 当前渲染区域的面选择角度扩散参数。
    property bool faceSelectByAngle: false
    property real faceSelectAngle: 30.0

    footer: ToolBar {
        id: toolbar
        height: 25
        RowLayout {
            anchors.fill: parent
            ToolButton {
                text: "几何面"
                checked: myItem.geometryStyle === 0 || myItem.geometryStyle === 1
                Layout.preferredWidth: 50
                Layout.fillHeight: true
                onClicked: geometryFaceMenu.open()
                Menu {
                    id: geometryFaceMenu
                    MenuItem {
                        text: "有边"
                        checkable: true
                        checked: myItem.geometryStyle === 0
                        onTriggered: myItem.setGeometryStyle(0)
                    }
                    MenuItem {
                        text: "无边"
                        checkable: true
                        checked: myItem.geometryStyle === 1
                        onTriggered: myItem.setGeometryStyle(1)
                    }
                }
            }
            ToolButton {
                text: "几何透"
                checked: myItem.geometryStyle >= 2 && myItem.geometryStyle <= 4
                Layout.preferredWidth: 50
                Layout.fillHeight: true
                onClicked: geometryTransMenu.open()
                Menu {
                    id: geometryTransMenu
                    MenuItem {
                        text: "75%"
                        checkable: true
                        checked: myItem.geometryStyle === 2
                        onTriggered: myItem.setGeometryStyle(2)
                    }
                    MenuItem {
                        text: "50%"
                        checkable: true
                        checked: myItem.geometryStyle === 3
                        onTriggered: myItem.setGeometryStyle(3)
                    }
                    MenuItem {
                        text: "25%"
                        checkable: true
                        checked: myItem.geometryStyle === 4
                        onTriggered: myItem.setGeometryStyle(4)
                    }
                }
            }
            ToolButton {
                text: "几何线"
                checked: myItem.geometryStyle === 5 || myItem.geometryStyle === 6
                Layout.preferredWidth: 50
                Layout.fillHeight: true
                onClicked: geometryWireMenu.open()
                Menu {
                    id: geometryWireMenu
                    MenuItem {
                        text: "带曲面线"
                        checkable: true
                        checked: myItem.geometryStyle === 5
                        onTriggered: myItem.setGeometryStyle(5)
                    }
                    MenuItem {
                        text: "无曲面线"
                        checkable: true
                        checked: myItem.geometryStyle === 6
                        onTriggered: myItem.setGeometryStyle(6)
                    }
                }
            }
            ToolButton {
                text: "几何隐"
                checkable: true
                checked: myItem.geometryStyle === 7
                Layout.preferredWidth: 50
                Layout.fillHeight: true
                onClicked: {
                    myItem.setGeometryStyle(myItem.geometryStyle === 7 ? 0 : 7)
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
                text: "比例尺"
                checkable: true
                Layout.preferredWidth: 50
                Layout.fillHeight: true
                onClicked: {
                    myItem.setScaleBarVisible(checked)
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

            Component.onCompleted: {
                myItem.setFaceSelectionByAngle(root.faceSelectByAngle, root.faceSelectAngle)
            }

            Connections {
                target: root
                function onFaceSelectByAngleChanged() {
                    myItem.setFaceSelectionByAngle(root.faceSelectByAngle, root.faceSelectAngle)
                }
                function onFaceSelectAngleChanged() {
                    myItem.setFaceSelectionByAngle(root.faceSelectByAngle, root.faceSelectAngle)
                }
            }

            onRightClicked: { viewportMenu.popup() }

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
                function onSelectionInvalidated() {
                    myItem.clearSelection()
                    selector.selection = null
                }
            }

            Connections {
                target: App
                function onModelVisibilityUpdated(modelId, visible) { myItem.setVisibility(modelId, visible) }
                function onComponentVisibilityUpdated(componentId, meshVisible, geometryVisible) {
                    myItem.setMeshVisibility(componentId, meshVisible)
                    myItem.setGeometryVisibility(componentId, geometryVisible)
                }
            }
        }

        Selector {
            id: selector
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.topMargin: 10
            anchors.leftMargin: 10

            faceSelectByAngle: root.faceSelectByAngle
            faceSelectAngle: root.faceSelectAngle

            onFaceSelectionSpreadEdited: function(enabled, angle) {
                root.faceSelectByAngle = enabled
                root.faceSelectAngle = angle
            }

            onClearButtonClicked: {
                myItem.clearSelection()
            }

            onConfirmButtonClicked: {
                selector.selection = myItem.selectedIDs
            }
        }

        Menu {
            id: viewportMenu

            property var pickedIds: []

            onOpened: {
                let sel = myItem.selectedIDs
                pickedIds = sel ? sel.getAsComponentIds() : []
            }

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
                shown: viewportMenu.pickedIds.length > 0
                onTriggered: {
                    for (let i = 0; i < viewportMenu.pickedIds.length; i++)
                        App.registry.objectTree.hideNode(viewportMenu.pickedIds[i], 1)
                }
            }

            StyledMenuItem {
                text: "隔离"
                shown: viewportMenu.pickedIds.length > 0
                onTriggered: {
                    if (viewportMenu.pickedIds.length > 0)
                        App.registry.objectTree.isolateSelection(viewportMenu.pickedIds)
                }
            }

            StyledMenuItem {
                text: "显示"
                shown: viewportMenu.pickedIds.length > 0
                onTriggered: {
                    for (let i = 0; i < viewportMenu.pickedIds.length; i++)
                        App.registry.objectTree.showNode(viewportMenu.pickedIds[i], 1)
                }
            }

            StyledSeparator {
                visible: viewportMenu.pickedIds.length > 0
            }

            StyledMenuItem {
                text: "全部隐藏"
                shown: viewportMenu.pickedIds.length === 0
                onTriggered: App.registry.objectTree.hideAllNodes()
            }

            StyledMenuItem {
                text: "全部显示"
                shown: viewportMenu.pickedIds.length === 0
                onTriggered: App.registry.objectTree.showAllNodes()
            }

            StyledMenuItem {
                text: "反转显示"
                shown: true
                onTriggered: App.registry.objectTree.reverseDisplayed()
            }
        }
    }

    Component.onCompleted: {
        App.registry.renderWindow = myItem
        // 注入功能系统适配器：startInteraction 按名称获取 InteractionState
        myItem.setFeatureAdaptor(QModelManager.featureSystem)
        Qt.callLater(function() { myItem.resetCamera() })
    }
}
