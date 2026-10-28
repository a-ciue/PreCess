import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import app.core
import app.model

Item {
    id: root
    clip: true

    property var attributes: []
    property int selectedIndex: -1
    property string componentName: ""
    property var scalarState: ({
            valid: false
        })
    property bool displayRangeMode: false
    property real sliderMinimum: 0
    property real sliderMaximum: 1
    property var pendingRange: null
    property string rangeError: ""
    property bool rangeFieldsDirty: false
    property var rangeEditTarget: null
    property var rememberedAttributes: ({})

    // 普通值截断至三位小数，极值用四位有效数字；按十进制文本截取，避免乘除造成浮点误差。
    function formatRangeValue(value) {
        if (!Number.isFinite(value))
            return "";
        let magnitude = Math.abs(value);
        if (magnitude !== 0 && (magnitude < 0.001 || magnitude >= 10000)) {
            let parts = magnitude.toExponential().split("e");
            let digits = (parts[0].replace(".", "") + "000").slice(0, 4);
            return (value < 0 ? "-" : "") + digits[0] + "." + digits.slice(1) + "e" + parts[1];
        }
        let text = String(value);
        let point = text.indexOf(".");
        return point < 0 ? text : text.slice(0, point + 4).replace(/\.?0+$/, "");
    }

    // 显示文本与实际值分开，缩短数字不改变渲染参数或另一端未编辑的精度。
    function updateRangeField(field, value) {
        field.exactValue = value;
        field.text = formatRangeValue(value);
    }

    // 恢复当前属性的两份独立范围，拖动期间固定滑条坐标，避免端点跳动。
    function refreshScalarState() {
        let attr = selectedAttribute();
        let window = App.registry.renderWindow;
        scalarState = attr && window && modeCombo.currentIndex === 1 ? window.scalarRangeState(App.selection.activeComponentId, attr.name, attr.attrType) : {
            valid: false
        };
        if (!scalarState.rangeAvailable)
            displayRangeMode = false;
        if (!scalarState.valid)
            return;
        let lower = displayRangeMode ? scalarState.displayLower : scalarState.mappingLower;
        let upper = displayRangeMode ? scalarState.displayUpper : scalarState.mappingUpper;
        if (!rangeMinField.activeFocus)
            updateRangeField(rangeMinField, lower);
        if (!rangeMaxField.activeFocus)
            updateRangeField(rangeMaxField, upper);
        if (!rangeSlider.first.pressed && !rangeSlider.second.pressed) {
            sliderMinimum = Math.min(scalarState.minimum, lower);
            sliderMaximum = Math.max(scalarState.maximum, upper);
            // 常量属性也需要可用的滑条跨度，但原始值域和重置数值保持不变。
            if (sliderMinimum === sliderMaximum) {
                let padding = Math.max(0.01, Math.abs(sliderMinimum) * 0.01);
                sliderMinimum -= padding;
                sliderMaximum += padding;
            }
            rangeSlider.setValues(lower, upper);
        }
    }

    // 合并连续拖动，仅将最新范围传给 GUI 管线；释放滑块时立即提交最后一个位置。
    function queueRange(lower, upper) {
        let attr = selectedAttribute();
        if (!attr || !scalarState.valid)
            return;
        pendingRange = {
            component: App.selection.activeComponentId,
            attribute: attr.name,
            type: attr.attrType,
            display: displayRangeMode,
            lower: lower,
            upper: upper
        };
        updateRangeField(rangeMinField, lower);
        updateRangeField(rangeMaxField, upper);
        if (!rangeUpdateTimer.running)
            rangeUpdateTimer.start();
    }

    // 请求携带捕获时的组件和属性身份，切换界面时不会写入另一属性。
    function flushPendingRange() {
        rangeUpdateTimer.stop();
        let request = pendingRange;
        pendingRange = null;
        if (request && App.registry.renderWindow)
            App.registry.renderWindow.setScalarRange(request.component, request.attribute, request.type, request.display, request.lower, request.upper);
    }

    // 记录编辑开始时的身份，组件切换后仍只向原属性保存参数。
    function markRangeEdited() {
        let attr = selectedAttribute();
        if (!rangeFieldsDirty && attr)
            rangeEditTarget = {
                component: App.selection.activeComponentId,
                attribute: attr.name,
                type: attr.attrType,
                display: displayRangeMode
            };
        rangeFieldsDirty = true;
    }

    // 编辑完成后先交换反向区间；仅提交实际编辑，模式切换和回填不能暗中启用筛选。
    function commitRangeFields() {
        flushPendingRange();
        if (!rangeFieldsDirty || !rangeEditTarget)
            return true;
        let lower = rangeMinField.exactValue;
        let upper = rangeMaxField.exactValue;
        if (scalarState.valid && (lower === null || upper === null)) {
            rangeError = "请输入有限数值";
            return false;
        }
        if (lower !== null && upper !== null && lower > upper) {
            let previousLower = lower;
            lower = upper;
            upper = previousLower;
            updateRangeField(rangeMinField, lower);
            updateRangeField(rangeMaxField, upper);
        }
        let target = rangeEditTarget;
        rangeFieldsDirty = false;
        rangeEditTarget = null;
        // 多分量属性沿用可选映射参数，不要求单值属性的范围状态。
        if (!scalarState.valid)
            return true;
        let accepted = App.registry.renderWindow.setScalarRange(target.component, target.attribute, target.type, target.display, lower, upper);
        rangeError = accepted ? "" : "当前属性不支持此范围";
        return accepted;
    }

    // 切换只改变编辑对象；切换前保存当前完整输入，不关闭另一种范围的效果。
    function changeRangeMode(displayMode) {
        if (!commitRangeFields())
            return;
        displayRangeMode = displayMode;
        rangeMinField.focus = false;
        rangeMaxField.focus = false;
        refreshScalarState();
    }

    // 按当前组件刷新属性目录，并恢复该组件上次选择的属性和策略。
    function refreshAttributes() {
        rangeFieldsDirty = false;
        rangeEditTarget = null;
        rangeMinField.focus = false;
        rangeMaxField.focus = false;
        selectedIndex = -1;
        if (App.selection.activeComponentId >= 0) {
            componentName = QModelManager.query.getComponentName(App.selection.activeComponentId);
            attributes = QModelManager.query.getComponentAttriInfo(App.selection.activeComponentId);
        } else {
            componentName = "";
            attributes = [];
        }
        let remembered = rememberedAttributes[String(App.selection.activeComponentId)];
        if (remembered) {
            for (let index = 0; index < attributes.length; ++index) {
                if (attributes[index].name === remembered.name && attributes[index].attrType === remembered.type) {
                    selectedIndex = index;
                    modeCombo.currentIndex = remembered.mode;
                    break;
                }
            }
        }
        refreshScalarState();
    }

    // 返回当前选中的属性条目，未选中时返回 null。
    function selectedAttribute() {
        if (selectedIndex < 0 || selectedIndex >= attributes.length)
            return null;
        return attributes[selectedIndex];
    }

    // 按属性分量数给一个常用默认模式，用户仍然可以手动切换。
    function inferMode(attr) {
        if (!attr)
            return 1;
        if (attr.componentCount === 2)
            return 2;
        if (attr.componentCount === 3)
            return 0;
        return 1;
    }

    // 只在输入框有内容时读取数值参数，空输入表示使用渲染策略默认值。
    function optionalNumber(text) {
        let trimmed = text.trim();
        if (trimmed.length === 0)
            return null;
        let value = Number(trimmed);
        return Number.isFinite(value) ? value : null;
    }

    // 组装 setAttriMode 的可选参数，保持和 QRenderWindow 控制台接口一致。
    function renderArgs() {
        let args = {};
        if (modeCombo.currentIndex === 1) {
            let attr = selectedAttribute();
            if (attr)
                args["element_type"] = attr.attrType;
            let minValue = rangeMinField.exactValue;
            let maxValue = rangeMaxField.exactValue;
            if (!scalarState.valid && minValue !== null && maxValue !== null)
                args["scalar_range"] = [minValue, maxValue];
        } else if (modeCombo.currentIndex === 2) {
            args["texture_path"] = texturePathField.text;
        } else if (modeCombo.currentIndex === 3) {
            let scale = optionalNumber(glyphScaleField.text);
            if (scale !== null)
                args["glyph_scale"] = scale;
        }
        return args;
    }

    function canApply() {
        let attr = selectedAttribute();
        if (!attr || !App.registry.renderWindow)
            return false;

        if (modeCombo.currentIndex === 0 && attr.componentCount !== 3)
            return false;
        if (modeCombo.currentIndex === 2 && (attr.attrType !== 0 || attr.componentCount !== 2))
            return false;
        if (modeCombo.currentIndex === 3 && attr.componentCount !== 3)
            return false;

        return modeCombo.currentIndex !== 2 || texturePathField.text.length > 0;
    }

    // 属性点击和应用按钮共用一次提交入口，沿用各渲染策略的参数检查。
    function applySelectedAttribute() {
        if (!canApply() || (modeCombo.currentIndex === 1 && !commitRangeFields()))
            return;
        let attr = selectedAttribute();
        App.registry.renderWindow.setAttriMode(App.selection.activeComponentId, attr.name, modeCombo.currentIndex, renderArgs());
    }

    function urlToPath(url) {
        let urlString = url.toString();
        if (urlString.startsWith("file:///")) {
            let start = urlString.charAt(9) === ":" ? 8 : 7;
            return decodeURIComponent(urlString.substring(start));
        }
        return decodeURIComponent(urlString);
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
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
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 60
            clip: true
            spacing: Theme.spacingXs
            model: root.attributes
            currentIndex: root.selectedIndex
            flickDeceleration: 100000
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                id: attrRow
                required property var modelData
                required property int index

                width: attributeList.width
                height: 34
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
                            visible: attrNameText.truncated && attributeMouse.hovered
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
                        if (!root.commitRangeFields())
                            return;
                        root.selectedIndex = index;
                        modeCombo.currentIndex = root.inferMode(modelData);
                        root.rememberedAttributes[String(App.selection.activeComponentId)] = {
                            name: modelData.name,
                            type: modelData.attrType,
                            mode: modeCombo.currentIndex
                        };
                        rangeMinField.focus = false;
                        rangeMaxField.focus = false;
                        root.rangeError = "";
                        root.refreshScalarState();
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
                    let attr = root.selectedAttribute();
                    if (attr)
                        root.rememberedAttributes[String(App.selection.activeComponentId)] = {
                            name: attr.name,
                            type: attr.attrType,
                            mode: currentIndex
                        };
                    root.refreshScalarState();
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.columnSpan: 2
                visible: modeCombo.currentIndex === 1
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    TextField {
                        id: rangeMinField
                        objectName: "scalarRangeLower"
                        // 实际值独立保留，获得焦点时仍维持精简显示。
                        property var exactValue: null
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        placeholderText: "min"
                        validator: DoubleValidator {}
                        onActiveFocusChanged: {
                            if (!activeFocus && !root.rangeFieldsDirty && exactValue !== null)
                                text = root.formatRangeValue(exactValue);
                        }
                        onTextEdited: {
                            exactValue = root.optionalNumber(text);
                            root.markRangeEdited();
                        }
                        onEditingFinished: {
                            if (root.commitRangeFields())
                                root.refreshScalarState();
                        }
                    }
                    Item {
                        Layout.preferredWidth: 24
                    }
                    TextField {
                        id: rangeMaxField
                        objectName: "scalarRangeUpper"
                        // 与下限独立保存精确值，避免仅编辑一端时截断另一端。
                        property var exactValue: null
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        horizontalAlignment: Text.AlignRight
                        placeholderText: "max"
                        validator: DoubleValidator {}
                        onActiveFocusChanged: {
                            if (!activeFocus && !root.rangeFieldsDirty && exactValue !== null)
                                text = root.formatRangeValue(exactValue);
                        }
                        onTextEdited: {
                            exactValue = root.optionalNumber(text);
                            root.markRangeEdited();
                        }
                        onEditingFinished: {
                            if (root.commitRangeFields())
                                root.refreshScalarState();
                        }
                    }
                }

                RangeSlider {
                    id: rangeSlider
                    objectName: "scalarRangeSlider"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    enabled: !!root.scalarState.valid
                    from: root.sliderMinimum
                    to: root.sliderMaximum
                    first.onMoved: root.queueRange(first.value, second.value)
                    second.onMoved: root.queueRange(first.value, second.value)
                    first.onPressedChanged: {
                        if (!first.pressed) {
                            root.flushPendingRange();
                            root.refreshScalarState();
                        }
                    }
                    second.onPressedChanged: {
                        if (!second.pressed) {
                            root.flushPendingRange();
                            root.refreshScalarState();
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Button {
                        objectName: "scalarMappingMode"
                        text: "映射范围"
                        highlighted: !root.displayRangeMode
                        onClicked: root.changeRangeMode(false)
                    }
                    Button {
                        objectName: "scalarDisplayMode"
                        text: "显示范围"
                        highlighted: root.displayRangeMode
                        enabled: !!root.scalarState.rangeAvailable
                        onClicked: root.changeRangeMode(true)
                        ToolTip.visible: hovered && !enabled
                        ToolTip.text: "仅边、面、体的单值属性支持范围隐藏"
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    Button {
                        objectName: "scalarRangeReset"
                        text: "重置"
                        enabled: !!root.scalarState.valid
                        onClicked: {
                            root.pendingRange = null;
                            rangeUpdateTimer.stop();
                            root.rangeFieldsDirty = false;
                            root.rangeEditTarget = null;
                            rangeMinField.focus = false;
                            rangeMaxField.focus = false;
                            let attr = root.selectedAttribute();
                            App.registry.renderWindow.resetScalarRange(App.selection.activeComponentId, attr.name, attr.attrType, root.displayRangeMode);
                            root.rangeError = "";
                            root.refreshScalarState();
                        }
                    }
                }

                Label {
                    objectName: "scalarOriginalRange"
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: Theme.textSecondary
                    text: root.rangeError.length > 0 ? root.rangeError : root.scalarState.valid ? "原始范围：" + root.formatRangeValue(root.scalarState.minimum) + " ～ " + root.formatRangeValue(root.scalarState.maximum) : "请选择有效的标量属性"
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

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSm

            Item {
                Layout.fillWidth: true
            }

            Button {
                id: applyButton
                objectName: "attributeRenderApply"
                text: "应用"
                enabled: root.canApply()
                // 主操作：实心强调色按钮
                background: Rectangle {
                    radius: Theme.radiusControl
                    color: !applyButton.enabled ? Theme.scrollBarIdle : applyButton.pressed ? Theme.primaryPressed : applyButton.hovered ? Theme.primaryHover : Theme.primary
                }
                contentItem: Text {
                    text: applyButton.text
                    color: applyButton.enabled ? Theme.textOnPrimary : Theme.textDisabled
                    font.pixelSize: Theme.fontSizeBody
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: root.applySelectedAttribute()
            }

            Button {
                objectName: "attributeRenderCancel"
                icon.source: "qrc:/images/toolbar/Panels/cancel-render.svg"
                icon.width: 14
                icon.height: 14
                icon.color: "transparent"
                text: "取消属性渲染"
                flat: true
                enabled: App.selection.activeComponentId >= 0 && App.registry.renderWindow
                onClicked: {
                    root.flushPendingRange();
                    App.registry.renderWindow.cancelComponentAttri(App.selection.activeComponentId);
                }
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

    Timer {
        id: rangeUpdateTimer
        interval: 16
        onTriggered: root.flushPendingRange()
    }

    Connections {
        target: App.registry.renderWindow
        function onScalarRangeChanged() {
            root.refreshScalarState();
        }
    }

    Connections {
        target: App.selection
        function onActiveComponentIdChanged() {
            root.commitRangeFields();
            root.refreshAttributes();
        }
    }

    Connections {
        target: QModelManager.observer
        function onComponentChanged(component_id) {
            if (component_id === App.selection.activeComponentId)
                root.refreshAttributes();
        }
    }

    Component.onCompleted: refreshAttributes()
}
