/**
 * @file SideBar.qml
 * @brief 侧边栏，执行复杂算法时提供参数的交互界面
 */

import QtQuick
import QtQuick.Shapes
import QtQuick.Controls.Fusion
import QtQuick.Layouts
import QtQuick.Dialogs

import app.core
import app.model
import app.model.systems.algo

Item{
    id: root
    required property OperationSession session
    readonly property var parameters: root.session.parameterModel.values
    readonly property QSelection emptySelection: root.session.parameterModel.emptySelection
    readonly property var activeOp: root.session.activeOperation
    readonly property bool meshGeneration: root.session.meshGeneration
    readonly property string panelTitle: root.session.panelTitle
    readonly property var meshAlgorithms: root.session.meshAlgorithms

    function setParam(index, value) {
        root.session.parameterModel.setValue(index, value)
    }

    ColumnLayout {
        id: meshHeader
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: root.meshGeneration ? Theme.spacingSm : 0
        spacing: Theme.spacingSm
        visible: root.meshGeneration
        height: visible ? implicitHeight : 0
        ComboBox {
            id: meshAlgorithmSelector
            objectName: "meshAlgorithmSelector"
            Layout.fillWidth: true
            // 单入口也保留选择框，让三级导航与当前算法身份始终可见。
            visible: root.meshAlgorithms.length > 0
            model: root.meshAlgorithms.map(info => ({ text: info.label || info.display_name, name: info.name }))
            textRole: "text"
            enabled: root.meshAlgorithms.length > 0
            currentIndex: root.activeOp && root.activeOp.info
                ? root.meshAlgorithms.findIndex(info => info.name === root.activeOp.info.name) : -1
            onActivated: index => root.session.selectMeshAlgorithm(index)
            Accessible.name: qsTr("网格生成算法")
        }
        Label {
            visible: root.meshAlgorithms.length === 0
            text: qsTr("暂无可用算法")
            color: Theme.textSecondary
            Layout.fillWidth: true
        }
    }

    RowLayout{
        id: buttonRow
        objectName: "operationButtons"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: visible ? Theme.spacingSm : 0
        height: visible ? 36 : 0
        spacing: Theme.spacingSm
        // 无活动操作时按钮行整体隐藏，避免两个 disabled 按钮占据首行
        visible: root.meshGeneration || !!(root.activeOp && root.activeOp.info)
        // AnchorChanges 显式撤销旧锚点，避免动态绑定留下上下双锚点撑满面板。
        states: State {
            name: "meshGeneration"
            when: root.meshGeneration
            AnchorChanges {
                target: buttonRow
                anchors.top: undefined
                anchors.bottom: root.bottom
            }
        }
        Button{
            id: commitButton
            text: "执行"
            enabled: !!(root.activeOp && root.activeOp.info)
            Layout.fillWidth: true
            Layout.fillHeight: true
            // 主操作：实心强调色按钮
            background: Rectangle {
                radius: Theme.radiusControl
                color: !commitButton.enabled ? Theme.scrollBarIdle
                     : commitButton.pressed ? Theme.primaryPressed
                     : commitButton.hovered ? Theme.primaryHover
                     : Theme.primary
            }
            contentItem: Text {
                text: commitButton.text
                color: commitButton.enabled ? Theme.textOnPrimary : Theme.textDisabled
                font.pixelSize: Theme.fontSizeBody
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked:{
                // 先停止参数监听，执行后的视口清空便不会反向清掉刚提交的参数。
                App.selection.listeningSelectorIndex = -1
                if (root.activeOp && root.activeOp.execute) {
                    try {
                        const result = root.activeOp.execute(App.selection.activeComponentId, root.parameters)
                        if (result !== undefined && result !== null)
                            QModelManager.taskStatus.showMessage(String(result))
                    } catch (error) {
                        QModelManager.taskStatus.reportFailure(String(error))
                    }
                }
                if (App.registry.renderWindow)
                    App.registry.renderWindow.clearSelection()
            }
        }
        Button{
            id: confirmButton
            text: "✓"
            Accessible.name: qsTr("结束当前操作")
            ToolTip.visible: hovered
            ToolTip.delay: 500
            ToolTip.text: qsTr("结束当前操作")
            enabled: !!root.activeOp
            Layout.fillHeight: true
            // 保留对号语义，用有边界的按钮与粗线标记提升辨识度。
            background: Rectangle {
                implicitWidth: 44
                radius: Theme.radiusControl
                color: confirmButton.down ? Theme.primaryTint
                     : confirmButton.hovered ? Theme.hoverOverlay : Theme.surface
                border.width: confirmButton.visualFocus ? 2 : 1
                border.color: confirmButton.visualFocus ? Theme.primary : Theme.textSecondary
            }
            contentItem: Item {
                implicitWidth: 24
                implicitHeight: 24
                // 直接绘制对号，避免字体替代导致笔画偏细。
                Shape {
                    anchors.centerIn: parent
                    width: 24
                    height: 24
                    ShapePath {
                        strokeColor: confirmButton.enabled ? Theme.primaryPressed : Theme.textDisabled
                        strokeWidth: 3
                        fillColor: "transparent"
                        capStyle: ShapePath.RoundCap
                        joinStyle: ShapePath.RoundJoin
                        startX: 5
                        startY: 12
                        PathLine { x: 10; y: 17 }
                        PathLine { x: 19; y: 7 }
                    }
                }
            }
            // 确认 = 结束当前操作，取消操作选中；再次执行需重新点选算法
            onClicked: App.activeOperation = null
        }
    }
    Item{
        anchors.top: root.meshGeneration ? meshHeader.bottom : buttonRow.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: root.meshGeneration ? buttonRow.top : parent.bottom
        clip: true
        ColumnLayout{
            anchors.fill: parent
            ListView{
                id:parameterList
                objectName: "operationParameters"
                clip: true
                // 固定预留滚动条槽，避免遮挡输入框或在滚动条显隐时挤动参数行。
                contentWidth: Math.max(0, width - parameterScrollBar.implicitWidth - Theme.spacingXs)
                // 所有参数行共用扣除滚动条后的标签列宽。
                readonly property real labelColumnWidth: contentWidth * 0.38
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.margins: Theme.spacingSm
                spacing: Theme.spacingSm
                model: root.activeOp && root.activeOp.info ? root.activeOp.info.arg_types : []
                delegate:Component{
                    Loader{
                        required property var model
                        required property int index
                        property bool initialized: status === Loader.Ready
                        sourceComponent:{
                            if(model.type === QArgType.Path){           //文件
                                return fileComponent
                            }
                            if(model.type === QArgType.Combo){           //多选一
                                return componentComboBox
                            }
                            if(model.type === QArgType.Float || model.type === QArgType.Int){ //数字框（浮点/整数）
                                return oneNumberBox
                            }
                            if(model.type === QArgType.Selector){           //选择器
                                return selectorComponent
                            }
                            if(model.type === QArgType.Text){           //文字输入框
                                return textComponent
                            }
                            if(model.type === QArgType.Bool){           //布尔值
                                return boolComponent
                            }
                            if(model.type === QArgType.Button){           //按钮
                                return buttonComponent
                            }
                        }
                    }
                }
                ScrollBar.vertical: ScrollBar {
                    id: parameterScrollBar
                    policy: ScrollBar.AsNeeded
                }
            }
        }
    }

    Component{
        id:componentComboBox
        RowLayout{
            id: comboRow
            // activated 的 index 是选项编号，不能当作参数在操作中的位置。
            readonly property int parameterIndex: index
            spacing: Theme.spacingSm
            width: parameterList.contentWidth
            readonly property var modelDataValues: model.content.split("|")[0].split(",").map(text => ({ text: text }))
            Text{
                id:nametext
                text: model.name
                Layout.preferredWidth: parameterList.labelColumnWidth
                elide: Text.ElideRight
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeBody
                verticalAlignment: Text.AlignVCenter
            }
            ComboBox{
                id:parameterComboBox
                objectName: "parameterControl_" + index
                Layout.fillWidth: true
                model: comboRow.modelDataValues
                textRole: "text"
                currentIndex: Number(root.parameters[comboRow.parameterIndex])
                onActivated: optionIndex => root.setParam(comboRow.parameterIndex, optionIndex)
            }
        }
    }
    Component{
        id:oneNumberBox
        RowLayout{
            spacing: Theme.spacingSm
            width: parameterList.contentWidth
            Text{
                id:nametext
                text: model.name
                Layout.preferredWidth: parameterList.labelColumnWidth
                elide: Text.ElideRight
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeBody
                verticalAlignment: Text.AlignVCenter
            }
            TextField {
                id:parameterTextInput
                objectName: "parameterControl_" + index
                Layout.minimumWidth: 0
                Layout.minimumHeight: 28
                padding: 6
                color: enabled ? Theme.textPrimary : Theme.textDisabled
                placeholderTextColor: Theme.textSecondary
                background: Rectangle {
                    radius: Theme.radiusControl
                    color: parent.enabled ? Theme.surface : Theme.surfaceAlt
                    border.width: parent.activeFocus ? 2 : 1
                    border.color: parent.activeFocus ? Theme.primary : Theme.borderStrong
                }
                Layout.fillWidth: true
                property bool editingValue: false
                readonly property string sourceText: Number.isNaN(root.parameters[index])
                        ? "" : String(root.parameters[index] ?? "")
                onSourceTextChanged: {
                    if (!editingValue)
                        text = sourceText
                }
                Component.onCompleted: text = sourceText
                onTextEdited: {
                    // 保留负号、小数点等中间文本，不用解析结果覆盖正在输入的内容。
                    editingValue = true
                    root.setParam(index, model.type === QArgType.Int ? parseInt(text) : parseFloat(text))
                    editingValue = false
                }
            }
        }
    }
    Component{
        id:fileComponent
        RowLayout{
            spacing: Theme.spacingSm
            width: parameterList.contentWidth
            Text{
                id:nametext
                text: model.name
                Layout.preferredWidth: parameterList.labelColumnWidth
                elide: Text.ElideRight
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeBody
                verticalAlignment: Text.AlignVCenter
            }
            TextArea{
                id:fileText
                objectName: "parameterControl_" + index
                Layout.minimumWidth: 0
                Layout.minimumHeight: 28
                padding: 6
                color: enabled ? Theme.textPrimary : Theme.textDisabled
                placeholderTextColor: Theme.textSecondary
                background: Rectangle {
                    radius: Theme.radiusControl
                    color: parent.enabled ? Theme.surface : Theme.surfaceAlt
                    border.width: parent.activeFocus ? 2 : 1
                    border.color: parent.activeFocus ? Theme.primary : Theme.borderStrong
                }
                wrapMode: TextEdit.Wrap
                Layout.fillWidth: true

                readonly property string sourceText: String(root.parameters[index] ?? "")
                onSourceTextChanged: text = sourceText
                Component.onCompleted: text = sourceText
                onTextChanged: {
                    if (parent.parent.initialized)
                        root.setParam(index, text)
                }
            }
            Button{
                icon.source: "qrc:/images/toolbar/Panels/browse-file.svg"
                icon.width: 14
                icon.height: 14
                icon.color: "transparent"
                text: qsTr("浏览…")
                flat: true
                onClicked:{
                    parameterFileDialog.open()
                }
            }
            FileDialog{
                id:parameterFileDialog
                onAccepted:{
                    fileText.text = urlToPath(selectedFile)
                    root.setParam(index, fileText.text)
                }

                function urlToPath(url) {
                    var urlString = new String(url)
                    var s
                    if (urlString.startsWith("file:///")) {
                        var k = urlString.charAt(9) === ':' ? 8 : 7
                        s = urlString.substring(k)
                    } else {
                        s = urlString
                    }
                    return decodeURIComponent(s);
                }
            }
        }
    }
    Component{
        id: textComponent
        RowLayout{
            id: textRow
            spacing: Theme.spacingSm
            width: parameterList.contentWidth
            Text{
                id:nametext
                text: model.name
                // 标签列与其余参数行同宽，拉伸面板后同步扩大
                Layout.preferredWidth: parameterList.labelColumnWidth
                elide: Text.ElideRight
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeBody
                verticalAlignment: Text.AlignVCenter
                ToolTip.visible: nameHover.hovered
                        && (truncated || model.description.length > 0)
                ToolTip.text: truncated
                        ? (model.description.length > 0
                            ? model.name + "\n" + model.description : model.name)
                        : model.description
                HoverHandler {
                    id: nameHover
                }
            }
            TextArea{
                id:fileText
                objectName: "parameterControl_" + index
                Layout.minimumWidth: 0
                Layout.minimumHeight: 28
                padding: 6
                color: enabled ? Theme.textPrimary : Theme.textDisabled
                placeholderTextColor: Theme.textSecondary
                background: Rectangle {
                    radius: Theme.radiusControl
                    color: parent.enabled ? Theme.surface : Theme.surfaceAlt
                    border.width: parent.activeFocus ? 2 : 1
                    border.color: parent.activeFocus ? Theme.primary : Theme.borderStrong
                }
                wrapMode: TextEdit.Wrap
                Layout.fillWidth: true
                placeholderText: model.description
                ToolTip.visible: hovered && model.description.length > 0
                ToolTip.text: model.description

                readonly property string sourceText: String(root.parameters[index] ?? "")
                onSourceTextChanged: text = sourceText
                Component.onCompleted: text = sourceText
                onTextChanged: {
                    if (textRow.parent.initialized)
                        root.setParam(index, text)
                }
            }
        }
    }
    Component{
        id: selectorComponent
        RowLayout{
            spacing: Theme.spacingSm
            width: parameterList.contentWidth
            readonly property var value: root.parameters[index]

            Text{
                id:nametext
                text: model.name
                Layout.preferredWidth: parameterList.labelColumnWidth
                elide: Text.ElideRight
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeBody
                verticalAlignment: Text.AlignVCenter
            }
            Text{
                id:selectedItems
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: value && value.size() > 0 ? value.size() : "无"
                color: value ? Theme.textPrimary : Theme.textSecondary
                font.pixelSize: Theme.fontSizeBody
                verticalAlignment: Text.AlignVCenter
            }

            Button{
                id: selectStartButton
                objectName: "parameterControl_" + index
                text: checked ? "结束选择" : "开始选择"
                // 拾取进行中：强调色提示当前面板处于监听状态
                highlighted: App.selection.listeningSelectorIndex === index
                checked: App.selection.listeningSelectorIndex === index
                onClicked: {
                    if (!checked) {
                        App.selection.listeningSelectorIndex = index
                        // 每次开始选择都按参数重设模式：content 指定的拾取类型优先；
                        // 未指定时纯几何（无网格）组件兜底几何点模式，其余兜底 Face
                        if (model.content.length > 0) {
                            const modes = model.content.split(",")
                            if (modes.length > 0)
                                App.selection.selectMode = modes[0]
                        } else {
                            const cid = App.selection.activeComponentId
                            const meshSummary = cid >= 0 ? QModelManager.query.getMeshSummary(cid) : ({})
                            const geomSummary = cid >= 0 ? QModelManager.query.getGeometrySummary(cid) : ({})
                            App.selection.selectMode = (!meshSummary.has_mesh && geomSummary.has_geometry) ? "GeometryVertex" : "Face"
                        }
                    } else {
                        App.selection.listeningSelectorIndex = -1
                    }
                }
            }

            Connections {
                target: App.selection
                enabled: selectStartButton.checked
                function onSelectionUpdated(selection) {
                    // 切换参数的同步通知可能先于 checked 绑定刷新，必须核对当前监听者。
                    if (App.selection.listeningSelectorIndex !== index)
                        return
                    // 只更新当前监听参数；视口的 null 在此转换为明确的空选择器。
                    root.setParam(index, selection === null ? root.emptySelection : selection)
                }
            }

        }
    }
    Component{
        id: boolComponent
        RowLayout{
            spacing: Theme.spacingSm
            width: parameterList.contentWidth

            Text{
                id:nametext
                text: model.name
                Layout.preferredWidth: parameterList.labelColumnWidth
                elide: Text.ElideRight
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeBody
                verticalAlignment: Text.AlignVCenter
            }
            CheckBox{
                id: parameterCheckBox
                objectName: "parameterControl_" + index
                // 加大勾选区域，以实色背景区分已选状态；保留控件自身的键盘和无障碍行为。
                indicator: Rectangle {
                    implicitWidth: 24
                    implicitHeight: 24
                    x: parameterCheckBox.leftPadding
                    y: parameterCheckBox.topPadding + (parameterCheckBox.availableHeight - height) / 2
                    radius: Theme.radiusControl
                    color: parameterCheckBox.checked ? (parameterCheckBox.enabled ? Theme.primaryPressed : Theme.textSecondary) : Theme.surface
                    border.width: 2
                    border.color: parameterCheckBox.checked || parameterCheckBox.visualFocus ? Theme.primaryPressed : Theme.textSecondary
                    Shape {
                        anchors.fill: parent
                        visible: parameterCheckBox.checked
                        ShapePath {
                            strokeColor: Theme.textOnPrimary
                            strokeWidth: 3
                            fillColor: "transparent"
                            capStyle: ShapePath.RoundCap
                            joinStyle: ShapePath.RoundJoin
                            startX: 5
                            startY: 12
                            PathLine { x: 10; y: 17 }
                            PathLine { x: 19; y: 7 }
                        }
                    }
                }

                checked: !!root.parameters[index]
                onToggled: root.setParam(index, checked)
            }
        }
    }
    Component{
        id: buttonComponent
        RowLayout{
            spacing: Theme.spacingSm
            width: parameterList.contentWidth
            Button{
                objectName: "parameterControl_" + index
                // Button 是无值触发器：计数器载荷，功能约定忽略值只读参数下标
                text: model.name
                Layout.fillWidth: true
                onClicked: root.setParam(index, (root.parameters[index] || 0) + 1)
            }
        }
    }
}
