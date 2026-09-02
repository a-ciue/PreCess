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

    // 当前渲染区域的拓扑诊断开关和二面角筛选范围。
    property real dihedralMinimumAngle: 0.0
    property real dihedralMaximumAngle: 150.0

    // 标记由功能自动开启的属性渲染，避免影响用户手动打开的属性渲染面板。
    property bool featureAttributeRenderingActive: false
    property int featureAttributeRenderingComponentId: -1

    // 结束当前功能自动开启的属性渲染，不影响用户手动控制的属性渲染面板。
    function cancelFeatureAttributeRendering() {
        if (!featureAttributeRenderingActive)
            return
        if (featureAttributeRenderingComponentId >= 0)
            myItem.cancelComponentAttri(featureAttributeRenderingComponentId)
        featureAttributeRenderingActive = false
        featureAttributeRenderingComponentId = -1
    }

    Connections {
        target: App
        function onActiveOperationChanged() {
            root.cancelFeatureAttributeRendering()
        }
    }

    Connections {
        target: QModelManager.featureSystem
        function onScalarAttributeDisplayRequested(componentId, attributeName) {
            root.featureAttributeRenderingActive = true
            root.featureAttributeRenderingComponentId = componentId
            // 使用标量模式显示质量属性，空参数表示采用默认标量范围。
            myItem.setAttriMode(componentId, attributeName, 1, {})
        }
    }

    footer: ToolBar {
        id: toolbar
        height: 25
        RowLayout {
            anchors.fill: parent

            ToolButton {
                id: geoBtn
                readonly property var geoLabels: [
                    "几何·面·有边", "几何·面·无边", "几何·透·75%", "几何·透·50%",
                    "几何·透·25%", "几何·线·带曲面线", "几何·线·无曲面线", "几何·隐"
                ]
                readonly property int menuItemHeight: 28
                readonly property int subMenuCloseDelay: 500
                text: myItem ? (myItem.geometryStyle >= 0 && myItem.geometryStyle < geoLabels.length ? geoLabels[myItem.geometryStyle] : "几何") : "几何"
                Layout.fillHeight: true
                onClicked: geoMenu.open()

                Timer { id: geoSubCloseTimer; interval: geoBtn.subMenuCloseDelay; onTriggered: { geoFaceMenu.close(); geoTransMenu.close(); geoWireMenu.close() } }

                Menu {
                    id: geoMenu
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
                    onAboutToShow: { y = -height }
                    onAboutToHide: {
                        geoFaceMenu.close()
                        geoTransMenu.close()
                        geoWireMenu.close()
                    }

                    MenuItem {
                        id: geoFaceCat
                        text: "面 ▶"
                        onHoveredChanged: {
                            if (hovered) {
                                geoSubCloseTimer.stop()
                                geoTransMenu.close()
                                geoWireMenu.close()
                                geoFaceMenu.popup(geoBtn, geoMenu.width, geoMenu.y + geoMenu.topPadding)
                            } else {
                                geoSubCloseTimer.restart()
                            }
                        }
                        onTriggered: geoFaceMenu.popup(geoBtn, geoMenu.width, geoMenu.y + geoMenu.topPadding)
                    }
                    MenuItem {
                        id: geoTransCat
                        text: "透 ▶"
                        onHoveredChanged: {
                            if (hovered) {
                                geoSubCloseTimer.stop()
                                geoFaceMenu.close()
                                geoWireMenu.close()
                                geoTransMenu.popup(geoBtn, geoMenu.width, geoMenu.y + geoMenu.topPadding + geoBtn.menuItemHeight)
                            } else {
                                geoSubCloseTimer.restart()
                            }
                        }
                        onTriggered: geoTransMenu.popup(geoBtn, geoMenu.width, geoMenu.y + geoMenu.topPadding + geoBtn.menuItemHeight)
                    }
                    MenuItem {
                        id: geoWireCat
                        text: "线 ▶"
                        onHoveredChanged: {
                            if (hovered) {
                                geoSubCloseTimer.stop()
                                geoFaceMenu.close()
                                geoTransMenu.close()
                                geoWireMenu.popup(geoBtn, geoMenu.width, geoMenu.y + geoMenu.topPadding + 2 * geoBtn.menuItemHeight)
                            } else {
                                geoSubCloseTimer.restart()
                            }
                        }
                        onTriggered: geoWireMenu.popup(geoBtn, geoMenu.width, geoMenu.y + geoMenu.topPadding + 2 * geoBtn.menuItemHeight)
                    }
                    MenuItem {
                        text: "隐"
                        checkable: true
                        checked: myItem.geometryStyle === 7
                        onTriggered: myItem.setGeometryStyle(myItem.geometryStyle === 7 ? 0 : 7)
                    }
                }

                Menu {
                    id: geoFaceMenu
                    MenuItem {
                        text: "有边"
                        checkable: true
                        checked: myItem.geometryStyle === 0
                        onHoveredChanged: { if (hovered) geoSubCloseTimer.stop() }
                        onTriggered: myItem.setGeometryStyle(0)
                    }
                    MenuItem {
                        text: "无边"
                        checkable: true
                        checked: myItem.geometryStyle === 1
                        onHoveredChanged: { if (hovered) geoSubCloseTimer.stop() }
                        onTriggered: myItem.setGeometryStyle(1)
                    }
                }
                Menu {
                    id: geoTransMenu
                    MenuItem {
                        text: "75%"
                        checkable: true
                        checked: myItem.geometryStyle === 2
                        onHoveredChanged: { if (hovered) geoSubCloseTimer.stop() }
                        onTriggered: myItem.setGeometryStyle(2)
                    }
                    MenuItem {
                        text: "50%"
                        checkable: true
                        checked: myItem.geometryStyle === 3
                        onHoveredChanged: { if (hovered) geoSubCloseTimer.stop() }
                        onTriggered: myItem.setGeometryStyle(3)
                    }
                    MenuItem {
                        text: "25%"
                        checkable: true
                        checked: myItem.geometryStyle === 4
                        onHoveredChanged: { if (hovered) geoSubCloseTimer.stop() }
                        onTriggered: myItem.setGeometryStyle(4)
                    }
                }
                Menu {
                    id: geoWireMenu
                    MenuItem {
                        text: "带曲面线"
                        checkable: true
                        checked: myItem.geometryStyle === 5
                        onHoveredChanged: { if (hovered) geoSubCloseTimer.stop() }
                        onTriggered: myItem.setGeometryStyle(5)
                    }
                    MenuItem {
                        text: "无曲面线"
                        checkable: true
                        checked: myItem.geometryStyle === 6
                        onHoveredChanged: { if (hovered) geoSubCloseTimer.stop() }
                        onTriggered: myItem.setGeometryStyle(6)
                    }
                }
            }

            ToolButton {
                id: meshBtn
                readonly property var meshLabels: [
                    "网格·面·带网格线", "网格·面·无线", "网格·透·75%", "网格·透·50%",
                    "网格·透·25%", "网格·线·带内部线", "网格·线·仅表面线", "网格·隐"
                ]
                readonly property int menuItemHeight: 28
                readonly property int subMenuCloseDelay: 500
                text: myItem ? (myItem.meshStyle >= 0 && myItem.meshStyle < meshLabels.length ? meshLabels[myItem.meshStyle] : "网格") : "网格"
                Layout.fillHeight: true
                onClicked: meshMenu.open()

                Timer { id: meshSubCloseTimer; interval: meshBtn.subMenuCloseDelay; onTriggered: { meshFaceMenu.close(); meshTransMenu.close(); meshWireMenu.close() } }

                Menu {
                    id: meshMenu
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
                    onAboutToShow: { y = -height }
                    onAboutToHide: {
                        meshFaceMenu.close()
                        meshTransMenu.close()
                        meshWireMenu.close()
                    }

                    MenuItem {
                        id: meshFaceCat
                        text: "面 ▶"
                        onHoveredChanged: {
                            if (hovered) {
                                meshSubCloseTimer.stop()
                                meshTransMenu.close()
                                meshWireMenu.close()
                                meshFaceMenu.popup(meshBtn, meshMenu.width, meshMenu.y + meshMenu.topPadding)
                            } else {
                                meshSubCloseTimer.restart()
                            }
                        }
                        onTriggered: meshFaceMenu.popup(meshBtn, meshMenu.width, meshMenu.y + meshMenu.topPadding)
                    }
                    MenuItem {
                        id: meshTransCat
                        text: "透 ▶"
                        onHoveredChanged: {
                            if (hovered) {
                                meshSubCloseTimer.stop()
                                meshFaceMenu.close()
                                meshWireMenu.close()
                                meshTransMenu.popup(meshBtn, meshMenu.width, meshMenu.y + meshMenu.topPadding + meshBtn.menuItemHeight)
                            } else {
                                meshSubCloseTimer.restart()
                            }
                        }
                        onTriggered: meshTransMenu.popup(meshBtn, meshMenu.width, meshMenu.y + meshMenu.topPadding + meshBtn.menuItemHeight)
                    }
                    MenuItem {
                        id: meshWireCat
                        text: "线 ▶"
                        onHoveredChanged: {
                            if (hovered) {
                                meshSubCloseTimer.stop()
                                meshFaceMenu.close()
                                meshTransMenu.close()
                                meshWireMenu.popup(meshBtn, meshMenu.width, meshMenu.y + meshMenu.topPadding + 2 * meshBtn.menuItemHeight)
                            } else {
                                meshSubCloseTimer.restart()
                            }
                        }
                        onTriggered: meshWireMenu.popup(meshBtn, meshMenu.width, meshMenu.y + meshMenu.topPadding + 2 * meshBtn.menuItemHeight)
                    }
                    MenuItem {
                        text: "隐"
                        checkable: true
                        checked: myItem.meshStyle === 7
                        onTriggered: myItem.setMeshStyle(myItem.meshStyle === 7 ? 0 : 7)
                    }
                }

                Menu {
                    id: meshFaceMenu
                    MenuItem {
                        text: "带网格线"
                        checkable: true
                        checked: myItem.meshStyle === 0
                        onHoveredChanged: { if (hovered) meshSubCloseTimer.stop() }
                        onTriggered: myItem.setMeshStyle(0)
                    }
                    MenuItem {
                        text: "无线"
                        checkable: true
                        checked: myItem.meshStyle === 1
                        onHoveredChanged: { if (hovered) meshSubCloseTimer.stop() }
                        onTriggered: myItem.setMeshStyle(1)
                    }
                }
                Menu {
                    id: meshTransMenu
                    MenuItem {
                        text: "75%"
                        checkable: true
                        checked: myItem.meshStyle === 2
                        onHoveredChanged: { if (hovered) meshSubCloseTimer.stop() }
                        onTriggered: myItem.setMeshStyle(2)
                    }
                    MenuItem {
                        text: "50%"
                        checkable: true
                        checked: myItem.meshStyle === 3
                        onHoveredChanged: { if (hovered) meshSubCloseTimer.stop() }
                        onTriggered: myItem.setMeshStyle(3)
                    }
                    MenuItem {
                        text: "25%"
                        checkable: true
                        checked: myItem.meshStyle === 4
                        onHoveredChanged: { if (hovered) meshSubCloseTimer.stop() }
                        onTriggered: myItem.setMeshStyle(4)
                    }
                }
                Menu {
                    id: meshWireMenu
                    MenuItem {
                        text: "带内部线"
                        checkable: true
                        checked: myItem.meshStyle === 5
                        onHoveredChanged: { if (hovered) meshSubCloseTimer.stop() }
                        onTriggered: myItem.setMeshStyle(5)
                    }
                    MenuItem {
                        text: "仅表面线"
                        checkable: true
                        checked: myItem.meshStyle === 6
                        onHoveredChanged: { if (hovered) meshSubCloseTimer.stop() }
                        onTriggered: myItem.setMeshStyle(6)
                    }
                }
            }

            ToolButton {
                id: topologyDiagnosticBtn
                text: "拓扑诊断"
                Layout.preferredWidth: 70
                Layout.fillHeight: true
                onClicked: topologyDiagnosticMenu.open()

                Menu {
                    id: topologyDiagnosticMenu
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
                    onAboutToShow: { y = -height }

                    function keepOpenAfterTrigger() {
                        Qt.callLater(function() {
                            if (!topologyDiagnosticMenu.visible)
                                topologyDiagnosticMenu.open()
                        })
                    }

                    MenuItem {
                        text: "边界边"
                        checkable: true
                        onToggled: myItem.setTopologyDiagnosticCategoryEnabled(0, checked)
                        onTriggered: topologyDiagnosticMenu.keepOpenAfterTrigger()
                    }
                    MenuItem {
                        text: "边界面"
                        checkable: true
                        onToggled: myItem.setTopologyDiagnosticCategoryEnabled(1, checked)
                        onTriggered: topologyDiagnosticMenu.keepOpenAfterTrigger()
                    }
                    MenuItem {
                        text: "非流形边"
                        checkable: true
                        onToggled: myItem.setTopologyDiagnosticCategoryEnabled(2, checked)
                        onTriggered: topologyDiagnosticMenu.keepOpenAfterTrigger()
                    }
                    MenuItem {
                        text: "非流形点"
                        checkable: true
                        onToggled: myItem.setTopologyDiagnosticCategoryEnabled(3, checked)
                        onTriggered: topologyDiagnosticMenu.keepOpenAfterTrigger()
                    }
                    MenuItem {
                        text: "孤立边"
                        checkable: true
                        onToggled: myItem.setTopologyDiagnosticCategoryEnabled(4, checked)
                        onTriggered: topologyDiagnosticMenu.keepOpenAfterTrigger()
                    }
                    MenuItem {
                        text: "孤立点"
                        checkable: true
                        onToggled: myItem.setTopologyDiagnosticCategoryEnabled(5, checked)
                        onTriggered: topologyDiagnosticMenu.keepOpenAfterTrigger()
                    }
                    MenuSeparator {}
                    MenuItem {
                        text: "二面角边"
                        checkable: true
                        onToggled: myItem.setTopologyDiagnosticCategoryEnabled(6, checked)
                        onTriggered: topologyDiagnosticMenu.keepOpenAfterTrigger()
                    }
                    MenuItem {
                        id: minimumDihedralItem
                        text: "最小二面角"
                        contentItem: RowLayout {
                            Label { text: minimumDihedralItem.text; Layout.fillWidth: true }
                            SpinBox {
                                from: 0
                                to: 180
                                editable: true
                                value: root.dihedralMinimumAngle
                                onValueModified: {
                                    root.dihedralMinimumAngle = value
                                    if (root.dihedralMaximumAngle < value)
                                        root.dihedralMaximumAngle = value
                                    myItem.setDihedralAngleRange(root.dihedralMinimumAngle,
                                                                 root.dihedralMaximumAngle)
                                }
                            }
                        }
                        onTriggered: topologyDiagnosticMenu.keepOpenAfterTrigger()
                    }
                    MenuItem {
                        id: maximumDihedralItem
                        text: "最大二面角"
                        contentItem: RowLayout {
                            Label { text: maximumDihedralItem.text; Layout.fillWidth: true }
                            SpinBox {
                                from: 0
                                to: 180
                                editable: true
                                value: root.dihedralMaximumAngle
                                onValueModified: {
                                    root.dihedralMaximumAngle = value
                                    if (root.dihedralMinimumAngle > value)
                                        root.dihedralMinimumAngle = value
                                    myItem.setDihedralAngleRange(root.dihedralMinimumAngle,
                                                                 root.dihedralMaximumAngle)
                                }
                            }
                        }
                        onTriggered: topologyDiagnosticMenu.keepOpenAfterTrigger()
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
