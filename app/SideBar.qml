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
import "AlgorithmNavigation.js" as AlgorithmNavigation

Item{
    id: root
    property var parameters: []
    property bool changingOperation: false
    readonly property QSelection emptySelection: QSelection {}

    readonly property var activeOp: App.activeOperation
    readonly property bool meshGeneration: !!(activeOp && activeOp.isMeshGeneration)
    readonly property string panelTitle: root.meshGeneration && root.activeOp.categoryTitle
            ? qsTr("操作面板") + "-" + root.activeOp.categoryTitle : qsTr("操作面板")
    property var algorithmInfos: QModelManager.algorithmSystem.algorithmsInfo
    readonly property var meshAlgorithms: meshGeneration
        ? AlgorithmNavigation.buildAlgorithms(root.algorithmInfos, activeOp.meshCategory) : []
    property var lastMeshAlgorithms: ({})

    function selectMeshAlgorithm(index, force = false) {
        if (!root.meshGeneration)
            return
        const info = index >= 0 && index < root.meshAlgorithms.length ? root.meshAlgorithms[index] : null
        // QVariant/QObject 包装可产生不同 JS 引用；用注册身份避免刷新循环。
        if (!force && info && root.activeOp.info && root.activeOp.info.name === info.name)
            return
        const category = root.activeOp.meshCategory
        const title = root.activeOp.categoryTitle
        if (info)
            root.lastMeshAlgorithms[category] = info.name
        App.activeOperation = {
            isMeshGeneration: true,
            meshCategory: category,
            categoryTitle: title,
            info: info,
            execute: info ? function(model, args) {
                QModelManager.algorithmSystem.call(info.name, model, args)
            } : null
        }
    }

    function refreshMeshAlgorithm(force = false) {
        if (!root.meshGeneration)
            return
        const name = root.activeOp.info ? root.activeOp.info.name
            : root.lastMeshAlgorithms[root.activeOp.meshCategory]
        const index = root.meshAlgorithms.findIndex(info => info.name === name)
        if (root.meshAlgorithms.length > 0)
            root.selectMeshAlgorithm(index >= 0 ? index : 0, force)
        else if (root.activeOp.info)
            root.selectMeshAlgorithm(-1)
    }

    function refreshRegisteredMeshAlgorithm() {
        root.refreshMeshAlgorithm(true)
    }

    onAlgorithmInfosChanged: Qt.callLater(root.refreshRegisteredMeshAlgorithm)

    // 参数模型与候选列表先完成绑定更新，再切换操作，避免重入清空新参数。
    onMeshAlgorithmsChanged: Qt.callLater(root.refreshMeshAlgorithm)

    onActiveOpChanged: {
        root.changingOperation = true
        App.selection.listeningSelectorIndex = -1
        // 操作开始时初始化全部参数，避免依赖可视行的创建与回收时机。
        parameters = root.createParameters()
        // 活动操作是功能则进入该功能（interactive 的交互随之一并上线），否则退出当前功能
        // （幂等，守卫在功能系统内；进入/退出经 FeatureHandler::activate/deactivate 通知功能）
        var isFeature = !!(activeOp && activeOp.isFeature)
        QModelManager.featureSystem.setFeatureActive(isFeature ? activeOp.info.name : "")
        if (isFeature) {
            for (let i = 0; i < root.parameters.length; ++i) {
                // Button 是点击事件，进入功能只初始化计数，不能派发一次点击。
                if (root.activeOp.info.arg_types[i].type !== QArgType.Button)
                    QModelManager.featureSystem.setParameter(root.activeOp.info.name, i, root.parameters[i])
            }
        }
        if (root.meshGeneration && !root.activeOp.info)
            Qt.callLater(root.refreshMeshAlgorithm)
        root.changingOperation = false
    }

    // 默认值属于操作会话，不能由可回收的 ListView 行反复写入。
    function createParameters() {
        if (!root.activeOp || !root.activeOp.info)
            return []
        const presets = root.meshGeneration && root.activeOp.info.category_defaults
                ? root.activeOp.info.category_defaults[root.activeOp.meshCategory] : null
        return root.activeOp.info.arg_types.map((arg, index) => {
            const defaults = root.activeOp.defaultParameters
            if (defaults && defaults[index] !== undefined)
                return defaults[index]
            const preset = presets ? presets[arg.name] : undefined
            const content = preset !== undefined ? String(preset) : arg.content
            switch (arg.type) {
            case QArgType.Combo: {
                const parts = arg.content.split("|")
                const count = parts[0].split(",").length
                const value = preset !== undefined ? Number(preset) : (parts.length > 1 ? parseInt(parts[1]) : 0)
                return Number.isFinite(value) && value >= 0 && value < count ? value : 0
            }
            case QArgType.Int: return parseInt(content)
            case QArgType.Float: return parseFloat(content)
            case QArgType.Bool: return content === "true"
            case QArgType.Selector: return root.emptySelection
            case QArgType.Button: return 0
            default: return content || ""
            }
        })
    }

    function updateParam(index, value) {
        if (Object.is(root.parameters[index], value))
            return false
        // 重新赋值才能通知绑定；原地修改 var 数组不会刷新功能回写的显示值。
        const values = root.parameters.slice()
        values[index] = value
        root.parameters = values
        return true
    }

    // 功能的参数持久化到系统；外部回写只更新展示，避免再次派发同一事件。
    function setParam(index, value) {
        // 切换时旧行尚未销毁，默认值刷新不能被当作对新操作的编辑。
        if (root.changingOperation)
            return
        if (root.updateParam(index, value) && root.activeOp && root.activeOp.isFeature)
            QModelManager.featureSystem.setParameter(root.activeOp.info.name, index, value)
    }

    // 清理全部选择器参数，包括 ListView 尚未创建的 delegate。
    Connections {
        target: App.selection
        function onSelectionInvalidated() {
            App.selection.listeningSelectorIndex = -1
            if (!root.activeOp || !root.activeOp.info)
                return
            const args = root.activeOp.info.arg_types
            for (let i = 0; i < args.length; ++i) {
                if (args[i].type === QArgType.Selector)
                    root.setParam(i, root.emptySelection)
            }
        }
    }

    // 功能侧回写参数值（如交互结果文本）→ 同步到面板显示
    Connections {
        target: QModelManager.featureSystem
        function onParamValueChanged(feature, index, value) {
            if (root.activeOp && root.activeOp.info && root.activeOp.info.name === feature)
                root.updateParam(index, value)
        }
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
            onActivated: index => root.selectMeshAlgorithm(index)
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
