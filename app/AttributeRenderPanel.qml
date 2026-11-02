import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import app.core
import app.model

Item {
    id: root
    clip: true
    // 宿主下限只保留可用视口和底部操作；完整内容高度独立用于滚动。
    implicitHeight: parameterViewport.implicitHeight + footerLayout.implicitHeight + 2 * parameterViewport.anchors.margins + panelLayout.spacing

    property var attributes: []
    property int selectedIndex: -1
    property string componentName: ""
    property var rangeState: App.registry.scalarRanges
    property string selectedKey: ""
    property int confirmedMode: 1
    property var rememberedAttributes: ({})
    property var rangeMetadata: ({ valid: false, rangeAvailable: false })
    readonly property var currentRange: rangeState ? rangeState.parameters(selectedKey) : ({
            valid: false
        })

    // 面板仅查询原始元信息，用户范围统一保存在界面参数对象中。
    function refreshRangeMetadata() {
        let attr = selectedAttribute();
        rangeMetadata = attr && rangeState ? rangeState.rangeInfo(App.selection.activeComponentId, attr.name, attr.attrType) : { valid: false, rangeAvailable: false };
        selectedKey = attr && rangeState ? rangeState.ensure(App.selection.activeComponentId, attr.name, attr.attrType, rangeMetadata) : "";
    }

    // 普通目录刷新恢复操作位置，同一属性的输入草稿由编辑器继续保留。
    function refreshAttributes() {
        selectedIndex = -1;
        let component = App.selection.activeComponentId;
        componentName = component >= 0 ? QModelManager.query.getComponentName(component) : "";
        attributes = component >= 0 ? QModelManager.query.getComponentAttriInfo(component) : [];
        let remembered = rememberedAttributes[String(component)];
        if (remembered) {
            for (let index = 0; index < attributes.length; ++index) {
                if (attributes[index].name === remembered.name && attributes[index].attrType === remembered.type) {
                    selectedIndex = index;
                    modeCombo.currentIndex = remembered.mode;
                    break;
                }
            }
        }
        confirmedMode = modeCombo.currentIndex;
        refreshRangeMetadata();
    }

    // 返回当前属性，避免目录刷新或无选中项时使用旧条目。
    function selectedAttribute() {
        return selectedIndex >= 0 && selectedIndex < attributes.length ? attributes[selectedIndex] : null;
    }

    // 保留多分量属性原有的默认渲染策略。
    function inferMode(attr) {
        return attr && attr.componentCount === 2 ? 2 : attr && attr.componentCount === 3 ? 0 : 1;
    }

    // 空输入沿用策略默认值，合法数值才作为可选参数发送。
    function optionalNumber(text) {
        let trimmed = text.trim();
        let value = trimmed.length > 0 ? Number(trimmed) : NaN;
        return Number.isFinite(value) ? value : null;
    }

    // 标量发送完整配置，其他策略保持已有参数入口。
    function renderArgs() {
        if (modeCombo.currentIndex === 2)
            return {
                texture_path: texturePathField.text
            };
        if (modeCombo.currentIndex === 3) {
            let scale = optionalNumber(glyphScaleField.text);
            return scale !== null ? {
                glyph_scale: scale
            } : {};
        }
        return {};
    }

    // 按属性分量和实体域限制原有 RGB、UV、向量策略。
    function canApply() {
        let attr = selectedAttribute();
        if (!attr || !App.registry.renderWindow || (modeCombo.currentIndex === 1 && !rangeState))
            return false;
        if (modeCombo.currentIndex === 0 && attr.componentCount !== 3)
            return false;
        if (modeCombo.currentIndex === 2 && (attr.attrType !== 0 || attr.componentCount !== 2))
            return false;
        if (modeCombo.currentIndex === 3 && attr.componentCount !== 3)
            return false;
        return modeCombo.currentIndex !== 2 || texturePathField.text.length > 0;
    }

    // 属性点击和应用共用入口，先完成草稿，再提交该属性的最终配置。
    function applySelectedAttribute() {
        if (!canApply())
            return;
        let edit = rangeEditor.takeParameters();
        if (edit === false)
            return;
        let attr = selectedAttribute();
        if (modeCombo.currentIndex === 1) {
            refreshRangeMetadata();
            rangeState.apply(selectedKey, edit ? edit.parameters : undefined);
        } else {
            if (rangeState) {
                rangeState.setActive(App.selection.activeComponentId, "", -1);
                if (edit)
                    rangeState.save(edit.key, edit.parameters);
            }
            App.registry.renderWindow.setAttriMode(App.selection.activeComponentId, attr.name, modeCombo.currentIndex, renderArgs());
        }
    }

    // 将文件选择器的 URL 转为既有渲染接口使用的本地路径。
    function urlToPath(url) {
        let text = url.toString();
        if (text.startsWith("file:///"))
            return decodeURIComponent(text.substring(text.charAt(9) === ":" ? 8 : 7));
        return decodeURIComponent(text);
    }

    FontMetrics {
        id: attributeFontMetrics
        font: Qt.application.font
    }

    Flickable {
        id: parameterViewport
        objectName: "attributeRenderScroll"
        // 为滚动操作保留 120 逻辑像素，字体放大时至少容纳两行属性。
        implicitHeight: Math.max(120, attributeList.implicitHeight)
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 6
        anchors.bottom: footerLayout.top
        contentWidth: width
        contentHeight: panelLayout.height
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        // 滚动条使用右侧留白，不挤压参数区，避免窄面板因滚动条出现而再次换行。
        ScrollBar.vertical: ScrollBar {
            parent: root
            anchors.top: parameterViewport.top
            anchors.bottom: parameterViewport.bottom
            anchors.right: parent.right
            width: parameterViewport.anchors.rightMargin
        }
    }

    ColumnLayout {
        id: panelLayout
        parent: parameterViewport.contentItem
        width: parameterViewport.width
        height: Math.max(implicitHeight, parameterViewport.height)
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            Layout.minimumHeight: implicitHeight
            spacing: Theme.spacingXs
            Label {
                text: "组件"
                color: Theme.textSecondary
                Layout.preferredWidth: 32
            }
            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: root.componentName.length > 0 ? root.componentName : "未选择"
                color: root.componentName.length > 0 ? Theme.textPrimary : Theme.textSecondary
                font.bold: root.componentName.length > 0
                elide: Text.ElideRight
            }
        }

        ListView {
            id: attributeList
            objectName: "attributeRenderList"
            // 两行使用与委托相同的行高，字体放大时不会只露出半行。
            readonly property real rowHeight: Math.max(34, attributeFontMetrics.height + 16)
            implicitHeight: 2 * rowHeight + spacing
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: implicitHeight
            clip: true
            spacing: Theme.spacingXs
            model: root.attributes
            currentIndex: root.selectedIndex
            interactive: contentHeight > height
            flickDeceleration: 100000
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                id: attrRow
                required property var modelData
                required property int index

                width: attributeList.width
                height: attributeList.rowHeight
                radius: Theme.radiusControl
                color: root.selectedIndex === index ? Theme.primaryTint : attributeMouse.hovered ? Theme.hoverOverlay : Theme.surface
                border.color: root.selectedIndex === index ? Theme.primary : Theme.border
                border.width: 1

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
                            visible: attrNameText.truncated && attributeMouse.containsMouse
                            text: attrNameText.text
                            delay: 300
                        }
                    }
                    Label {
                        text: modelData.componentCount > 0 ? modelData.componentCount + "分量" : ""
                        elide: Text.ElideRight
                    }
                }

                MouseArea {
                    id: attributeMouse
                    objectName: "attributeRenderSelect"
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        if (!rangeEditor.finishEditing())
                            return;
                        root.selectedIndex = index;
                        modeCombo.currentIndex = root.inferMode(modelData);
                        root.rememberedAttributes[String(App.selection.activeComponentId)] = {
                            name: modelData.name,
                            type: modelData.attrType,
                            mode: modeCombo.currentIndex
                        };
                        root.confirmedMode = modeCombo.currentIndex;
                        root.refreshRangeMetadata();
                        root.applySelectedAttribute();
                    }
                }
            }

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                padding: 0
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

        GridLayout {
            Layout.fillWidth: true
            Layout.minimumHeight: implicitHeight
            columns: 2
            rowSpacing: 6
            columnSpacing: 6

            Label {
                text: "渲染策略"
            }
            ComboBox {
                id: modeCombo
                objectName: "attributeRenderMode"
                Layout.fillWidth: true
                model: ["RGB", "标量", "UV", "向量"]
                onActivated: {
                    if (!rangeEditor.finishEditing()) {
                        currentIndex = root.confirmedMode;
                        return;
                    }
                    root.confirmedMode = currentIndex;
                    let attr = root.selectedAttribute();
                    if (attr)
                        root.rememberedAttributes[String(App.selection.activeComponentId)] = {
                            name: attr.name,
                            type: attr.attrType,
                            mode: currentIndex
                        };
                    root.refreshRangeMetadata();
                }
            }

            ScalarRangeEditor {
                id: rangeEditor
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.columnSpan: 2
                visible: modeCombo.currentIndex === 1
                attributeKey: root.selectedKey
                parameters: root.currentRange
                metadata: root.rangeMetadata
                onParametersEdited: function (key, parameters) {
                    if (root.rangeState)
                        root.rangeState.update(key, parameters);
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
                    icon.source: "qrc:/images/toolbar/Panels/texture.svg"
                    icon.width: 14
                    icon.height: 14
                    icon.color: "transparent"
                    text: "..."
                    flat: true
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
    }

    RowLayout {
        id: footerLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 6
        height: implicitHeight
        spacing: Theme.spacingSm

        Item {
            Layout.fillWidth: true
        }

        Button {
            id: cancelButton
            objectName: "attributeRenderCancel"
            Layout.alignment: Qt.AlignRight
            Layout.minimumHeight: 32
            leftPadding: Theme.spacingXs
            rightPadding: Theme.spacingXs
            text: qsTr("取消属性渲染")
            font.pixelSize: Theme.fontSizeBody
            // 鼠标点击不抢输入焦点，由取消入口只保存草稿；键盘仍可 Tab 聚焦。
            focusPolicy: Qt.TabFocus
            palette.button: Theme.surfaceAlt
            palette.buttonText: Theme.textPrimary
            palette.mid: Theme.border
            palette.light: Theme.surface
            palette.dark: Theme.borderStrong
            palette.highlight: Theme.primary
            flat: true
            enabled: App.selection.activeComponentId >= 0 && App.registry.renderWindow
            ToolTip.visible: hovered || visualFocus
            ToolTip.text: qsTr("关闭属性着色和范围筛选，保留参数")
            ToolTip.delay: 400
            onClicked: {
                let edit = rangeEditor.takeParameters();
                if (root.rangeState)
                    root.rangeState.cancel(App.selection.activeComponentId, edit ? edit.key : "", edit ? edit.parameters : undefined);
            }
        }

        Button {
            id: applyButton
            objectName: "attributeRenderApply"
            Layout.alignment: Qt.AlignRight
            implicitWidth: Math.max(64, implicitContentWidth + leftPadding + rightPadding)
            implicitHeight: Math.max(32, implicitContentHeight + topPadding + bottomPadding)
            padding: Theme.spacingSm
            text: qsTr("应用")
            font.pixelSize: Theme.fontSizeBody
            // 避免鼠标按下时先失焦应用，点击入口统一完成一次最终提交。
            focusPolicy: Qt.TabFocus
            enabled: root.canApply()
            ToolTip.visible: hovered || visualFocus
            ToolTip.text: qsTr("重新应用当前属性和参数；选择属性时也会自动应用")
            ToolTip.delay: 400
            // 主操作保留明暗主题的对比度，同时显示键盘焦点。
            background: Rectangle {
                radius: Theme.radiusControl
                color: !applyButton.enabled ? Theme.surfaceAlt : Theme.dark ? (applyButton.down ? Theme.hoverOverlay : Theme.primaryTint) : applyButton.down ? Theme.primaryPressed : applyButton.hovered ? Theme.primaryHover : Theme.primary
                border.width: applyButton.visualFocus ? 2 : 1
                border.color: applyButton.visualFocus ? Theme.textPrimary : applyButton.enabled ? Theme.primary : Theme.border
            }
            contentItem: Text {
                text: applyButton.text
                color: !applyButton.enabled ? Theme.textDisabled : Theme.dark ? Theme.textPrimary : Theme.textOnPrimary
                font: applyButton.font
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: root.applySelectedAttribute()
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
        target: App.registry.renderWindow
        function onScalarRangeChanged() {
            root.refreshRangeMetadata();
        }
    }

    Connections {
        target: App.selection
        function onActiveComponentIdChanged() {
            rangeEditor.finishEditing();
            root.refreshAttributes();
        }
    }

    Connections {
        target: QModelManager.observer
        function onComponentRemoved(component_id) {
            delete root.rememberedAttributes[String(component_id)];
            if (component_id === App.selection.activeComponentId)
                root.refreshAttributes();
        }
        function onComponentChanged(component_id) {
            if (component_id === App.selection.activeComponentId)
                root.refreshAttributes();
        }
    }

    Component.onCompleted: refreshAttributes()
}
