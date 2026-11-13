/**
 * @file ScalarRangeEditor.qml
 * @brief 标量范围输入、精确值草稿及双滑块交互，不依赖模型查询或渲染服务。
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    objectName: "scalarRangeEditor"
    spacing: 4

    property string attributeKey: ""
    property var parameters: ({})
    property var metadata: ({
            valid: false,
            rangeAvailable: false
        })
    property bool displayMode: false
    // 草稿与有效待发送请求各自携带目标身份，属性切换不会把旧值写入新属性。
    property var draft: null
    property var pendingRange: null
    property var dragStart: null
    // 原生端点手势的身份保持到释放，不随当前属性或范围模式切换。
    property var handleEditTarget: null
    property string rangeError: ""
    property real sliderMinimum: 0
    property real sliderMaximum: 1

    signal parametersEdited(string targetKey, var parameters)

    // 普通值截断至三位小数，极值用四位有效数字，格式化不改写保存精度。
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

    // 空值保留策略默认值，未完成的文本不会被转换成零。
    function optionalNumber(text) {
        let trimmed = text.trim();
        let value = trimmed.length ? Number(trimmed) : NaN;
        return Number.isFinite(value) ? value : null;
    }

    // 仅首次编辑捕获精确的另一端，不从已格式化的输入文本反推数值。
    function editField(lowerField, text) {
        flushPendingRange();
        if (!attributeKey)
            return;
        let edit = draft || {
            key: attributeKey,
            display: displayMode,
            lower: displayMode ? parameters.displayLower : parameters.mappingLower,
            upper: displayMode ? parameters.displayUpper : parameters.mappingUpper,
            hasRange: !!metadata.valid,
            rangeAvailable: !!metadata.rangeAvailable,
            parameters: Object.assign({}, parameters)
        };
        let updated = Object.assign({}, edit);
        if (lowerField)
            updated.lower = optionalNumber(text);
        else
            updated.upper = optionalNumber(text);
        draft = updated;
        rangeError = "";
    }

    // 普通失焦或拖动交付一次完整参数；显式应用和取消直接取值，不发送中间编辑。
    function finishEditing() {
        if (!draft && !pendingRange)
            return true;
        let result = takeParameters();
        if (result === false)
            return false;
        if (result)
            parametersEdited(result.key, result.parameters);
        return true;
    }

    // 从手势开始时的参数计算完整结果，未编辑端保持原始精度和目标身份。
    function takeParameters() {
        let edit = draft || pendingRange;
        if (!edit)
            return attributeKey ? {
                key: attributeKey,
                parameters: Object.assign({}, parameters)
            } : null;
        let lower = edit.lower;
        let upper = edit.upper;
        if (edit.hasRange && (!Number.isFinite(lower) || !Number.isFinite(upper))) {
            rangeError = qsTr("请输入有限数值");
            return false;
        }
        if (lower !== null && upper !== null && lower > upper) {
            let previous = lower;
            lower = upper;
            upper = previous;
        }
        let next = Object.assign({}, edit.parameters);
        if (edit.display || (next.linked && edit.rangeAvailable)) {
            next.filtered = next.filtered || edit.display || next.displayLower !== lower || next.displayUpper !== upper;
            next.displayLower = lower;
            next.displayUpper = upper;
        }
        if (!edit.display || next.linked) {
            next.mappingLower = lower;
            next.mappingUpper = upper;
        }
        draft = null;
        pendingRange = null;
        rangeUpdateTimer.stop();
        rangeError = "";
        if (edit.key === attributeKey) {
            rangeMinField.text = formatRangeValue(lower);
            rangeMaxField.text = formatRangeValue(upper);
        }
        return {
            key: edit.key,
            parameters: next
        };
    }

    // 草稿与联动变化合成同一份最终参数；解绑不会漏掉仍按原联动规则产生的编辑。
    function toggleLinked() {
        let result = takeParameters();
        if (!result || result.key !== attributeKey || !metadata.rangeAvailable)
            return;
        let next = result.parameters;
        next.linked = !next.linked;
        if (next.linked) {
            let lower = displayMode ? next.displayLower : next.mappingLower;
            let upper = displayMode ? next.displayUpper : next.mappingUpper;
            next.filtered = next.filtered || next.displayLower !== lower || next.displayUpper !== upper;
            next.mappingLower = next.displayLower = lower;
            next.mappingUpper = next.displayUpper = upper;
        }
        parametersEdited(result.key, next);
    }

    // 重置丢弃未完成输入，按当前元信息恢复选定范围，联动时同时恢复两种范围。
    function resetParameters() {
        if (!attributeKey || !metadata.valid)
            return;
        pendingRange = null;
        rangeUpdateTimer.stop();
        draft = null;
        rangeError = "";
        rangeMinField.focus = false;
        rangeMaxField.focus = false;
        let next = Object.assign({}, parameters);
        if (displayMode || next.linked) {
            next.displayLower = metadata.minimum;
            next.displayUpper = metadata.maximum;
            next.filtered = false;
        }
        if (!displayMode || next.linked) {
            next.mappingLower = metadata.minimum;
            next.mappingUpper = metadata.maximum;
        }
        parametersEdited(attributeKey, next);
        refreshValues();
    }

    // 同一属性的刷新不覆盖未提交草稿；拖动时固定轨道坐标。
    function refreshValues() {
        if (!metadata.rangeAvailable)
            displayMode = false;
        let lower = displayMode ? parameters.displayLower : parameters.mappingLower;
        let upper = displayMode ? parameters.displayUpper : parameters.mappingUpper;
        if (!draft) {
            if (!rangeMinField.activeFocus)
                rangeMinField.text = formatRangeValue(lower);
            if (!rangeMaxField.activeFocus)
                rangeMaxField.text = formatRangeValue(upper);
        }
        if (!metadata.valid || rangeSlider.first.pressed || rangeSlider.second.pressed || dragStart)
            return;
        sliderMinimum = Math.min(metadata.minimum, lower);
        sliderMaximum = Math.max(metadata.maximum, upper);
        if (sliderMinimum === sliderMaximum) {
            let padding = Math.max(0.01, Math.abs(sliderMinimum) * 0.01);
            sliderMinimum -= padding;
            sliderMaximum += padding;
        }
        rangeSlider.setValues(lower, upper);
    }

    // 首次按下捕获端点身份；旧手势结束后才恢复新属性的滑条数值。
    function syncHandleEditTarget() {
        if (rangeSlider.first.pressed || rangeSlider.second.pressed) {
            if (!handleEditTarget)
                handleEditTarget = {
                    key: attributeKey,
                    display: displayMode
                };
        } else {
            flushPendingRange();
            handleEditTarget = null;
            refreshValues();
        }
    }

    // 切换目标后忽略旧端点手势，仅合并属于当前目标的有效请求。
    function queueRange(lower, upper) {
        if (!attributeKey || !metadata.valid)
            return;
        let target = handleEditTarget;
        if (target && (target.key !== attributeKey || target.display !== displayMode))
            return;
        if (draft && !finishEditing())
            return;
        pendingRange = {
            key: target ? target.key : attributeKey,
            display: target ? target.display : displayMode,
            lower: lower,
            upper: upper,
            hasRange: true,
            rangeAvailable: !!metadata.rangeAvailable,
            parameters: Object.assign({}, parameters)
        };
        rangeMinField.text = formatRangeValue(lower);
        rangeMaxField.text = formatRangeValue(upper);
        if (!rangeUpdateTimer.running)
            rangeUpdateTimer.start();
    }

    // 延迟请求保留捕获时的完整参数和身份，不再向外传播应用策略标志。
    function flushPendingRange() {
        rangeUpdateTimer.stop();
        if (pendingRange)
            finishEditing();
    }

    // 模式只改变编辑对象，不能隐式启用或停用另一份范围。
    function changeRangeMode(display) {
        if (!finishEditing())
            return;
        displayMode = display;
        rangeMinField.focus = false;
        rangeMaxField.focus = false;
        refreshValues();
    }

    onAttributeKeyChanged: {
        // 外部切换已发生时尽力提交旧目标，非法草稿不带入新属性。
        finishEditing();
        draft = null;
        dragStart = null;
        rangeError = "";
        rangeMinField.focus = false;
        rangeMaxField.focus = false;
        rangeMinField.text = "";
        rangeMaxField.text = "";
        refreshValues();
    }
    onParametersChanged: refreshValues()
    onMetadataChanged: refreshValues()
    Component.onCompleted: refreshValues()

    Timer {
        id: rangeUpdateTimer
        interval: 16
        onTriggered: root.flushPendingRange()
    }

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

    RowLayout {
        objectName: "scalarOriginalRange"
        Layout.fillWidth: true
        visible: !!root.metadata.valid
        spacing: Theme.spacingSm

        Label {
            objectName: "scalarOriginalLower"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            color: Theme.textSecondary
            text: root.metadata.valid ? root.formatRangeValue(root.metadata.minimum) : ""
            Accessible.name: qsTr("原始范围下限：%1").arg(text)
        }
        Label {
            objectName: "scalarOriginalUpper"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            horizontalAlignment: Text.AlignRight
            color: Theme.textSecondary
            text: root.metadata.valid ? root.formatRangeValue(root.metadata.maximum) : ""
            Accessible.name: qsTr("原始范围上限：%1").arg(text)
        }
    }

    RangeSlider {
        id: rangeSlider
        objectName: "scalarRangeSlider"
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        enabled: !!root.metadata.valid
        from: root.sliderMinimum
        to: root.sliderMaximum
        first.onMoved: root.queueRange(first.value, second.value)
        second.onMoved: root.queueRange(first.value, second.value)
        first.onPressedChanged: root.syncHandleEditTarget()
        second.onPressedChanged: root.syncHandleEditTarget()

        // 区间内部在按下时接管鼠标，避免控件把最近端点跳到点击位置。
        MouseArea {
            id: rangePan
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            preventStealing: true
            cursorShape: Qt.SizeHorCursor

            // 结束平移后立即提交最后位置，再恢复滑条坐标范围的自动更新。
            function finishDrag() {
                if (!root.dragStart)
                    return;
                root.flushPendingRange();
                root.dragStart = null;
                root.refreshValues();
            }

            onPressed: function (mouse) {
                let first = rangeSlider.first.handle;
                let second = rangeSlider.second.handle;
                let left = Math.min(first.x + first.width, second.x + second.width);
                let right = Math.max(first.x, second.x);
                if (mouse.x <= left || mouse.x >= right) {
                    mouse.accepted = false;
                    return;
                }
                if (!root.finishEditing())
                    return;
                root.refreshValues();
                root.dragStart = {
                    x: mouse.x,
                    lower: rangeSlider.first.value,
                    upper: rangeSlider.second.value
                };
            }
            onPositionChanged: function (mouse) {
                if (!root.dragStart)
                    return;
                let width = rangeSlider.availableWidth - Math.max(rangeSlider.first.handle.width, rangeSlider.second.handle.width);
                if (width <= 0)
                    return;
                let offset = (mouse.x - root.dragStart.x) / width * (rangeSlider.to - rangeSlider.from) * (rangeSlider.mirrored ? -1 : 1);
                // 两端一起夹到轨道边界，保持原区间宽度。
                offset = Math.max(rangeSlider.from - root.dragStart.lower, Math.min(rangeSlider.to - root.dragStart.upper, offset));
                if (rangeSlider.first.value === root.dragStart.lower + offset && rangeSlider.second.value === root.dragStart.upper + offset)
                    return;
                rangeSlider.setValues(root.dragStart.lower + offset, root.dragStart.upper + offset);
                root.queueRange(rangeSlider.first.value, rangeSlider.second.value);
            }
            onReleased: finishDrag()
            onCanceled: finishDrag()
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.spacingSm

        TextField {
            id: rangeMinField
            objectName: "scalarRangeLower"
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.minimumWidth: 0
            placeholderText: "min"
            horizontalAlignment: Text.AlignHCenter
            Accessible.name: root.displayMode ? qsTr("可见单元下限") : qsTr("颜色映射下限")
            validator: DoubleValidator {}
            onTextEdited: root.editField(true, text)
            onEditingFinished: Qt.callLater(() => root.finishEditing())
        }
        TextField {
            id: rangeMaxField
            objectName: "scalarRangeUpper"
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.minimumWidth: 0
            placeholderText: "max"
            horizontalAlignment: Text.AlignHCenter
            Accessible.name: root.displayMode ? qsTr("可见单元上限") : qsTr("颜色映射上限")
            validator: DoubleValidator {}
            onTextEdited: root.editField(false, text)
            onEditingFinished: Qt.callLater(() => root.finishEditing())
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
            implicitWidth: mappingModeButton.implicitWidth + rangeLinkButton.implicitWidth + displayModeButton.implicitWidth + 8
            implicitHeight: Math.max(mappingModeButton.implicitHeight, rangeLinkButton.implicitHeight, displayModeButton.implicitHeight) + 4
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
                    // 联动时共同高亮，解绑后仍由当前编辑模式决定单选。
                    selected: !!root.parameters.linked || !root.displayMode
                    enabled: !!root.metadata.valid
                    onClicked: root.changeRangeMode(false)
                    Keys.onRightPressed: {
                        if (displayModeButton.enabled) {
                            displayModeButton.forceActiveFocus();
                            root.changeRangeMode(true);
                        }
                    }
                    ToolTip.visible: hovered || visualFocus
                    ToolTip.text: root.parameters.linked ? qsTr("编辑颜色映射范围，同步可见单元范围") : qsTr("编辑颜色映射范围，保留可见单元范围")
                    ToolTip.delay: 400
                }
                ToolButton {
                    id: rangeLinkButton
                    objectName: "scalarRangeLink"
                    implicitWidth: 32
                    implicitHeight: 32
                    padding: 6
                    // 鼠标点击由按钮完成提交，保留输入焦点；键盘仍可通过 Tab 聚焦。
                    focusPolicy: Qt.TabFocus
                    icon.width: 20
                    icon.height: 20
                    icon.source: root.parameters.linked ? "qrc:/images/toolbar/Panels/range-linked.svg" : "qrc:/images/toolbar/Panels/range-unlinked.svg"
                    icon.color: !enabled ? Theme.textDisabled : root.parameters.linked ? Theme.primary : Theme.textSecondary
                    enabled: !!root.metadata.valid && !!root.metadata.rangeAvailable
                    Accessible.role: Accessible.CheckBox
                    Accessible.checkable: true
                    Accessible.checked: !!root.parameters.linked
                    Accessible.name: root.parameters.linked ? qsTr("解除范围绑定") : qsTr("绑定颜色映射与可见单元范围")
                    ToolTip.visible: hovered || visualFocus
                    ToolTip.text: !root.metadata.rangeAvailable ? qsTr("此属性不支持范围绑定") : root.parameters.linked ? qsTr("已绑定：两种范围同步修改，点击解除绑定") : qsTr("以当前范围为准，绑定颜色映射与可见单元范围")
                    ToolTip.delay: 400
                    onClicked: root.toggleLinked()
                }
                RangeModeButton {
                    id: displayModeButton
                    objectName: "scalarDisplayMode"
                    Layout.fillWidth: true
                    Layout.minimumWidth: implicitWidth
                    Layout.preferredWidth: 1
                    text: qsTr("可见单元")
                    selected: !!root.parameters.linked || root.displayMode
                    enabled: !!root.metadata.rangeAvailable
                    onClicked: root.changeRangeMode(true)
                    Keys.onLeftPressed: {
                        mappingModeButton.forceActiveFocus();
                        root.changeRangeMode(false);
                    }
                    Accessible.description: qsTr("隐藏范围外的边、面或体单元；节点属性不支持此操作")
                    ToolTip.visible: hovered || visualFocus
                    ToolTip.text: root.parameters.linked ? qsTr("编辑可见单元范围，同步颜色映射范围") : qsTr("编辑可见单元范围，隐藏范围外的单元并保留颜色映射")
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
            // 重置先丢弃草稿，鼠标按下时不能因抢焦点提前应用。
            focusPolicy: Qt.TabFocus
            palette.button: Theme.surfaceAlt
            palette.buttonText: Theme.textPrimary
            palette.mid: Theme.border
            palette.light: Theme.surface
            palette.dark: Theme.borderStrong
            palette.highlight: Theme.primary
            enabled: !!root.metadata.valid
            Accessible.name: root.parameters.linked ? qsTr("重置两种范围") : root.displayMode ? qsTr("重置可见单元范围") : qsTr("重置颜色映射范围")
            ToolTip.visible: hovered || visualFocus
            ToolTip.text: root.parameters.linked ? qsTr("将已绑定的两种范围一起恢复为原始值域") : root.displayMode ? qsTr("将可见单元范围恢复为原始值域，保留颜色映射") : qsTr("将颜色映射范围恢复为原始值域，保留可见单元范围")
            ToolTip.delay: 400
            onClicked: root.resetParameters()
        }
    }

    Label {
        Layout.fillWidth: true
        visible: root.rangeError.length > 0 || !root.metadata.valid
        wrapMode: Text.Wrap
        color: Theme.textSecondary
        text: root.rangeError || qsTr("请选择有效的标量属性")
    }
}
