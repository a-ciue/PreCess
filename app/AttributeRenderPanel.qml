import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import QtQuick.Dialogs

import app.core
import app.model

Item {
    id: root
    clip: true

    property var attributes: []
    property int selectedIndex: -1
    property int componentId: -1
    property string componentName: ""
    property string componentSelectionMessage: ""
    // 与 SideBar 参数索引错开，避免两个选择器同时接收视口确认结果。
    readonly property int componentSelectorIndex: 2147483647

    // 根据属性面板自己的目标组件重新读取属性列表，并清空已选属性。
    function refreshAttributes() {
        selectedIndex = -1
        if (componentId >= 0) {
            componentName = QModelManager.query.getComponentName(componentId)
            attributes = QModelManager.query.getComponentAttriInfo(componentId)
        } else {
            componentName = ""
            attributes = []
        }
    }

    // 设置属性列表和渲染操作共同使用的目标 Component。
    function setComponent(targetComponentId) {
        if (App.selection.listeningSelectorIndex === componentSelectorIndex)
            App.selection.listeningSelectorIndex = -1
        componentId = targetComponentId
        componentSelectionMessage = ""
        refreshAttributes()
    }

    // 返回当前选中的属性条目，未选中时返回 null。
    function selectedAttribute() {
        if (selectedIndex < 0 || selectedIndex >= attributes.length)
            return null
        return attributes[selectedIndex]
    }

    // 按属性分量数给一个常用默认模式，用户仍然可以手动切换。
    function inferMode(attr) {
        if (!attr)
            return 1
        if (attr.componentCount === 2)
            return 2
        if (attr.componentCount === 3)
            return 0
        return 1
    }

    // 只在输入框有内容时读取数值参数，空输入表示使用渲染策略默认值。
    function optionalNumber(text) {
        let trimmed = text.trim()
        if (trimmed.length === 0)
            return null
        let value = Number(trimmed)
        return Number.isFinite(value) ? value : null
    }

    // 组装 setAttriMode 的可选参数，保持和 QRenderWindow 控制台接口一致。
    function renderArgs() {
        let args = {}
        if (modeCombo.currentIndex === 1) {
            let minValue = optionalNumber(rangeMinField.text)
            let maxValue = optionalNumber(rangeMaxField.text)
            if (minValue !== null && maxValue !== null)
                args["scalar_range"] = [minValue, maxValue]
        } else if (modeCombo.currentIndex === 2) {
            args["texture_path"] = texturePathField.text
        } else if (modeCombo.currentIndex === 3) {
            let scale = optionalNumber(glyphScaleField.text)
            if (scale !== null)
                args["glyph_scale"] = scale
        }
        return args
    }

    function canApply() {
        let attr = selectedAttribute()
        if (componentId < 0 || !attr || !attr.renderable || !App.registry.renderWindow)
            return false

        if (modeCombo.currentIndex === 0 && attr.componentCount !== 3)
            return false
        if (modeCombo.currentIndex === 2
                && (attr.attrType !== 0 || attr.componentCount !== 2))
            return false
        if (modeCombo.currentIndex === 3 && attr.componentCount !== 3)
            return false

        return modeCombo.currentIndex !== 2 || texturePathField.text.length > 0
    }

    function urlToPath(url) {
        let urlString = url.toString()
        if (urlString.startsWith("file:///")) {
            let start = urlString.charAt(9) === ":" ? 8 : 7
            return decodeURIComponent(urlString.substring(start))
        }
        return decodeURIComponent(urlString)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 6

        ListView {
            id: attributeList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 60
            clip: true
            model: root.attributes
            currentIndex: root.selectedIndex
            flickDeceleration: 100000
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                required property var modelData
                required property int index

                width: attributeList.width
                height: 34
                color: root.selectedIndex === index ? "#cfe8ff" : "transparent"
                border.color: "#d0d0d0"
                border.width: 1
                opacity: modelData.renderable ? 1.0 : 0.45

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 14
                    spacing: 8

                    Label {
                        text: modelData.typeName
                        Layout.preferredWidth: 24
                    }
                    Label {
                        id: attrNameText
                        text: modelData.displayName
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0

                        ToolTip {
                            visible: attrNameText.truncated && attributeMouse.hovered
                            text: attrNameText.text
                            delay: 300
                        }
                    }
                    Label {
                        text: modelData.renderable
                            ? (modelData.componentCount > 0 ? modelData.componentCount + "分量" : "")
                            : "暂不支持"
                        elide: Text.ElideRight
                    }
                }

                MouseArea {
                    id: attributeMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: modelData.renderable
                    onClicked: {
                        root.selectedIndex = index
                        modeCombo.currentIndex = root.inferMode(modelData)
                    }
                }
            }

            ScrollBar.vertical: Basic.ScrollBar {
                policy: ScrollBar.AsNeeded
                padding: 0
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

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            rowSpacing: 6
            columnSpacing: 6

           RowLayout {
                Layout.columnSpan: 2
                Layout.fillWidth: true
                Layout.preferredHeight: componentSelectStartButton.implicitHeight
                Layout.maximumHeight: componentSelectStartButton.implicitHeight
                spacing: 5

                Text {
                    text: "组件"
                }
                Rectangle {
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1
                    color: "black"
                }
                Text {
                    text: root.componentName.length > 0 ? root.componentName : "无"
                }
                Button {
                    id: componentSelectStartButton
                    text: "开始选择"
                    checked: App.selection.listeningSelectorIndex === root.componentSelectorIndex
                    onClicked: {
                        if (!checked) {
                            root.componentSelectionMessage = ""
                            App.selection.listeningSelectorIndex = root.componentSelectorIndex
                            App.selection.selectMode = "Component"
                        } else {
                            App.selection.listeningSelectorIndex = -1
                        }
                    }
                }

                Connections {
                    target: App.selection
                    enabled: componentSelectStartButton.checked
                    function onConfirmed(selection) {
                        const componentIds = selection ? selection.getAsComponentIds() : []
                        if (componentIds.length === 1)
                            root.setComponent(componentIds[0])
                        else
                            root.componentSelectionMessage = "只能选择一个 Component"
                        App.selection.listeningSelectorIndex = -1
                    }
                }

                Connections {
                    target: App.selection
                    function onSelectionInvalidated() {
                        if (App.selection.listeningSelectorIndex === root.componentSelectorIndex)
                            App.selection.listeningSelectorIndex = -1
                    }
                }
            }

            Label {
                Layout.columnSpan: 2
                visible: root.componentSelectionMessage.length > 0
                text: root.componentSelectionMessage
                color: "#c62828"
            }

            Label {
                text: "渲染策略"
            }
            ComboBox {
                id: modeCombo
                Layout.fillWidth: true
                model: ["RGB", "标量", "UV", "向量"]
            }

            Label {
                text: "范围"
                visible: modeCombo.currentIndex === 1
                Layout.preferredWidth: 32
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                visible: modeCombo.currentIndex === 1
                TextField {
                    id: rangeMinField
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    placeholderText: "min"
                    validator: DoubleValidator {}
                }
                TextField {
                    id: rangeMaxField
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    placeholderText: "max"
                    validator: DoubleValidator {}
                }
            }

            Label {
                text: "贴图"
                visible: modeCombo.currentIndex === 2
                Layout.preferredWidth: 32
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                visible: modeCombo.currentIndex === 2
                TextField {
                    id: texturePathField
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                }
                Button {
                    text: "..."
                    Layout.preferredWidth: 28
                    onClicked: textureFileDialog.open()
                }
            }

            Label {
                text: "缩放"
                visible: modeCombo.currentIndex === 3
                Layout.preferredWidth: 32
            }
            TextField {
                id: glyphScaleField
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                visible: modeCombo.currentIndex === 3
                validator: DoubleValidator {
                    bottom: 0
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Button {
                text: "应用"
                enabled: root.canApply()
                onClicked: {
                    let attr = root.selectedAttribute()
                    if (attr)
                        App.registry.renderWindow.setAttriMode(
                            root.componentId, attr.name, modeCombo.currentIndex, root.renderArgs())
                }
            }

            Button {
                text: "取消属性渲染"
                enabled: root.componentId >= 0 && App.registry.renderWindow
                onClicked: App.registry.renderWindow.cancelComponentAttri(root.componentId)
            }
        }
    }

    FileDialog {
        id: textureFileDialog
        title: "选择贴图"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Image files (*.png *.jpg *.jpeg *.bmp)", "All files (*)"]
        onAccepted: texturePathField.text = root.urlToPath(selectedFile)
    }

    Connections {
        target: QModelManager.observer
        function onComponentChanged(component_id) {
            if (component_id === root.componentId)
                root.refreshAttributes()
        }
        function onComponentRemoved(component_id) {
            if (component_id === root.componentId)
                root.setComponent(-1)
        }
    }

    Component.onCompleted: refreshAttributes()
}
