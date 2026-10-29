import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import app.core
import app.model

Item {
    id: root
    clip: true
    // 声明完整内容所需高度；外层分配不足时滚动参数，底部操作保持可见。
    implicitHeight: panelLayout.implicitHeight + footerLayout.implicitHeight + 2 * parameterViewport.anchors.margins + panelLayout.spacing

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
    property var rangeEditTarget: null
    property var rememberedAttributes: ({})

    // 范围模式由面板状态驱动，切换校验失败时保持原选中项。
    component RangeModeButton: ToolButton {
        id: control
        property bool selected: false

        implicitHeight: Math.max(32, implicitContentHeight + topPadding + bottomPadding)
        padding: Theme.spacingSm
        font.pixelSize: Theme.fontSizeBody
        font.weight: selected ? Font.DemiBold : Font.Normal
        focusPolicy: Qt.StrongFocus
        Accessible.role: Accessible.PageTab
        Accessible.checkable: true
        Accessible.checked: selected
        Accessible.name: text

        background: Rectangle {
            radius: Math.max(0, Theme.radiusControl - 1)
            color: control.down ? Theme.hoverOverlay : control.selected ? Theme.surface : control.hovered ? Theme.hoverOverlay : "transparent"
            border.width: control.visualFocus ? 2 : 0
            border.color: Theme.primary

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 4
                height: 2
                visible: control.selected
                color: control.enabled ? Theme.primary : Theme.textDisabled
            }
        }
        contentItem: Text {
            text: control.text
            font: control.font
            color: !control.enabled ? Theme.textDisabled : control.selected || control.hovered ? Theme.textPrimary : Theme.textSecondary
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

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

    // 输入身份包含组件、实体域和属性名，多分量属性也有独立归属。
    function rangeAttributeKey() {
        let attr = selectedAttribute();
        return attr ? JSON.stringify([App.selection.activeComponentId, attr.attrType, attr.name]) : "";
    }

    // 切换属性时放弃旧输入；普通状态刷新不调用此入口。
    function clearRangeFields() {
        rangeEditTarget = null;
        rangeMinField.exactValue = null;
        rangeMaxField.exactValue = null;
        rangeMinField.text = "";
        rangeMaxField.text = "";
        rangeError = "";
        rangeMinField.focus = false;
        rangeMaxField.focus = false;
    }

    // 恢复当前属性的两份独立范围，拖动期间固定滑条坐标，避免端点跳动。
    function refreshScalarState() {
        let attr = selectedAttribute();
        let window = App.registry.renderWindow;
        let attributeKey = rangeAttributeKey();
        let attributeChanged = scalarState.attributeKey !== attributeKey;
        if (attributeChanged)
            commitRangeFields();
        let state = attr && window && modeCombo.currentIndex === 1 ? window.scalarRangeState(App.selection.activeComponentId, attr.name, attr.attrType) : {
            valid: false
        };
        // 范围状态携带输入归属，无后端范围时仍能识别真正的属性切换。
        state.attributeKey = attributeKey;
        scalarState = state;
        if (attributeChanged)
            clearRangeFields();
        if (!scalarState.rangeAvailable)
            displayRangeMode = false;
        if (!scalarState.valid)
            return;
        let lower = displayRangeMode ? scalarState.displayLower : scalarState.mappingLower;
        let upper = displayRangeMode ? scalarState.displayUpper : scalarState.mappingUpper;
        if (!rangeEditTarget) {
            if (!rangeMinField.activeFocus)
                updateRangeField(rangeMinField, lower);
            if (!rangeMaxField.activeFocus)
                updateRangeField(rangeMaxField, upper);
        }
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
        if (!rangeEditTarget && attr)
            rangeEditTarget = {
                component: App.selection.activeComponentId,
                attribute: attr.name,
                type: attr.attrType,
                display: displayRangeMode,
                hasRange: !!scalarState.valid
            };
    }

    // 编辑完成后先交换反向区间；仅提交实际编辑，模式切换和回填不能暗中启用筛选。
    function commitRangeFields() {
        flushPendingRange();
        if (!rangeEditTarget)
            return true;
        let target = rangeEditTarget;
        let lower = rangeMinField.exactValue;
        let upper = rangeMaxField.exactValue;
        if (target.hasRange && (lower === null || upper === null)) {
            rangeError = qsTr("请输入有限数值");
            return false;
        }
        if (lower !== null && upper !== null && lower > upper) {
            let previousLower = lower;
            lower = upper;
            upper = previousLower;
            updateRangeField(rangeMinField, lower);
            updateRangeField(rangeMaxField, upper);
        }
        // 提交前移出待编辑状态，后端同步通知可直接回填；拒绝时恢复目标供用户纠正。
        rangeEditTarget = null;
        let window = App.registry.renderWindow;
        let accepted = !target.hasRange || !!window && window.setScalarRange(target.component, target.attribute, target.type, target.display, lower, upper);
        if (!accepted)
            rangeEditTarget = target;
        rangeError = accepted ? "" : qsTr("当前属性不支持此范围");
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
            if (scalarState.attributeKey === rangeAttributeKey() && !scalarState.valid && minValue !== null && maxValue !== null)
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

    FontMetrics {
        id: attributeFontMetrics
        font: Qt.application.font
    }

    Flickable {
        id: parameterViewport
        objectName: "attributeRenderScroll"
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
                    objectName: "scalarOriginalRange"
                    Layout.fillWidth: true
                    visible: !!root.scalarState.valid
                    spacing: Theme.spacingSm

                    Label {
                        objectName: "scalarOriginalLower"
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        color: Theme.textSecondary
                        text: root.scalarState.valid ? root.formatRangeValue(root.scalarState.minimum) : ""
                        Accessible.name: qsTr("原始范围下限：%1").arg(text)
                    }
                    Label {
                        objectName: "scalarOriginalUpper"
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        horizontalAlignment: Text.AlignRight
                        color: Theme.textSecondary
                        text: root.scalarState.valid ? root.formatRangeValue(root.scalarState.maximum) : ""
                        Accessible.name: qsTr("原始范围上限：%1").arg(text)
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
                    spacing: Theme.spacingSm

                    TextField {
                        id: rangeMinField
                        objectName: "scalarRangeLower"
                        // 实际值独立保留，获得焦点时仍维持精简显示。
                        property var exactValue: null
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        Layout.minimumWidth: 0
                        placeholderText: "min"
                        horizontalAlignment: Text.AlignHCenter
                        Accessible.name: root.displayRangeMode ? qsTr("可见单元下限") : qsTr("颜色映射下限")
                        validator: DoubleValidator {}
                        onActiveFocusChanged: {
                            if (!activeFocus && !root.rangeEditTarget && exactValue !== null)
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
                    Label {
                        objectName: "scalarRangeLabel"
                        text: qsTr("范围")
                        color: Theme.textSecondary
                    }
                    TextField {
                        id: rangeMaxField
                        objectName: "scalarRangeUpper"
                        // 与下限独立保存精确值，避免仅编辑一端时截断另一端。
                        property var exactValue: null
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        Layout.minimumWidth: 0
                        placeholderText: "max"
                        horizontalAlignment: Text.AlignHCenter
                        Accessible.name: root.displayRangeMode ? qsTr("可见单元上限") : qsTr("颜色映射上限")
                        validator: DoubleValidator {}
                        onActiveFocusChanged: {
                            if (!activeFocus && !root.rangeEditTarget && exactValue !== null)
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

                GridLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    columns: width >= rangeModeGroup.implicitWidth + rangeResetButton.implicitWidth + columnSpacing ? 2 : 1
                    columnSpacing: Theme.spacingSm
                    rowSpacing: Theme.spacingXs

                    Rectangle {
                        id: rangeModeGroup
                        Layout.fillWidth: true
                        implicitWidth: mappingModeButton.implicitWidth + displayModeButton.implicitWidth + 6
                        implicitHeight: Math.max(mappingModeButton.implicitHeight, displayModeButton.implicitHeight) + 4
                        radius: Theme.radiusControl
                        color: Theme.surfaceAlt
                        border.color: Theme.border

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 2
                            spacing: 2

                            RangeModeButton {
                                id: mappingModeButton
                                objectName: "scalarMappingMode"
                                Layout.fillWidth: true
                                Layout.minimumWidth: implicitWidth
                                Layout.preferredWidth: 1
                                text: qsTr("颜色映射")
                                selected: !root.displayRangeMode
                                enabled: !!root.scalarState.valid
                                onClicked: root.changeRangeMode(false)
                                Keys.onRightPressed: {
                                    if (displayModeButton.enabled) {
                                        displayModeButton.forceActiveFocus();
                                        root.changeRangeMode(true);
                                    }
                                }
                                ToolTip.visible: hovered || visualFocus
                                ToolTip.text: qsTr("编辑颜色映射范围，保留可见单元范围")
                                ToolTip.delay: 400
                            }
                            RangeModeButton {
                                id: displayModeButton
                                objectName: "scalarDisplayMode"
                                Layout.fillWidth: true
                                Layout.minimumWidth: implicitWidth
                                Layout.preferredWidth: 1
                                text: qsTr("可见单元")
                                selected: root.displayRangeMode
                                enabled: !!root.scalarState.rangeAvailable
                                onClicked: root.changeRangeMode(true)
                                Keys.onLeftPressed: {
                                    mappingModeButton.forceActiveFocus();
                                    root.changeRangeMode(false);
                                }
                                Accessible.description: qsTr("隐藏范围外的边、面或体单元；节点属性不支持此操作")
                                ToolTip.visible: hovered || visualFocus
                                ToolTip.text: qsTr("编辑可见单元范围，隐藏范围外的单元并保留颜色映射")
                                ToolTip.delay: 400
                            }
                        }
                    }
                    Button {
                        id: rangeResetButton
                        objectName: "scalarRangeReset"
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        Layout.minimumHeight: 32
                        Layout.preferredHeight: rangeModeGroup.implicitHeight
                        implicitWidth: Math.max(72, implicitContentWidth + leftPadding + rightPadding)
                        leftPadding: Theme.spacingSm
                        rightPadding: Theme.spacingSm
                        text: qsTr("重置范围")
                        font.pixelSize: Theme.fontSizeBody
                        focusPolicy: Qt.StrongFocus
                        palette.button: Theme.surfaceAlt
                        palette.buttonText: Theme.textPrimary
                        palette.mid: Theme.border
                        palette.light: Theme.surface
                        palette.dark: Theme.borderStrong
                        palette.highlight: Theme.primary
                        enabled: !!root.scalarState.valid && !!App.registry.renderWindow
                        Accessible.name: root.displayRangeMode ? qsTr("重置可见单元范围") : qsTr("重置颜色映射范围")
                        ToolTip.visible: hovered || visualFocus
                        ToolTip.text: root.displayRangeMode ? qsTr("将可见单元范围恢复为原始值域，保留颜色映射") : qsTr("将颜色映射范围恢复为原始值域，保留可见单元范围")
                        ToolTip.delay: 400
                        onClicked: {
                            let window = App.registry.renderWindow;
                            let attr = root.selectedAttribute();
                            if (!window || !attr)
                                return;
                            root.pendingRange = null;
                            rangeUpdateTimer.stop();
                            root.rangeEditTarget = null;
                            rangeMinField.focus = false;
                            rangeMaxField.focus = false;
                            window.resetScalarRange(App.selection.activeComponentId, attr.name, attr.attrType, root.displayRangeMode);
                            root.rangeError = "";
                            root.refreshScalarState();
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.rangeError.length > 0 || !root.scalarState.valid
                    wrapMode: Text.Wrap
                    color: Theme.textSecondary
                    text: root.rangeError.length > 0 ? root.rangeError : qsTr("请选择有效的标量属性")
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
            focusPolicy: Qt.StrongFocus
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
                root.flushPendingRange();
                App.registry.renderWindow.cancelComponentAttri(App.selection.activeComponentId);
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
            focusPolicy: Qt.StrongFocus
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
