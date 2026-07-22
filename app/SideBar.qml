/**
 * @file SideBar.qml
 * @brief 侧边栏，执行复杂算法时提供参数的交互界面
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import app.core
import app.model
import app.model.systems.algo

Item{
    id: root
    property var parameters: []
    property var resultText: ""

    readonly property var activeOp: App.activeOperation

    onActiveOpChanged: {
        parameters = []
        resultText = ""
    }

    // 写入参数值；功能的参数为持久参数，修改即时写回功能系统实时生效
    function setParam(index, value) {
        parameters[index] = value
        if (root.activeOp && root.activeOp.isFeature)
            QModelManager.featureSystem.setParameter(root.activeOp.info.name, index, value)
    }

    Button{
        id: commitButton
        text: "执行"
        enabled: !!(root.activeOp && root.activeOp.info)
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height:30
        onClicked:{
            if (root.activeOp && root.activeOp.execute)
                root.resultText = root.activeOp.execute(App.selection.activeComponentId, root.parameters)
            if (App.registry.renderWindow)
                App.registry.renderWindow.clearSelection()
        }
    }
    TextArea {
        id: resultArea
        anchors.top: commitButton.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        // 不可见时不占锚定布局高度，避免留下空白
        height: visible ? 80 : 0
        readOnly: true
        text: root.resultText
        wrapMode: TextEdit.Wrap
        visible: text.length > 0
        background: Rectangle {
            color: "#f0f0f0"
            border.color: "#d0d0d0"
            border.width: 1
        }
    }
    Item{
        anchors.top: resultArea.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
        ColumnLayout{
            anchors.fill: parent
            ListView{
                id:parameterList
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.margins: 3
                spacing: 5
                model: root.activeOp ? root.activeOp.info.arg_types : []
                delegate:Component{
                    Loader{
                        required property var model
                        required property int index
                        sourceComponent:{
                            if(model.type === QArgType.Path){           //文件
                                return fileComponent
                            }
                            if(model.type === QArgType.Combo){           //多选一
                                return componentComboBox
                            }
                            if(model.type === QArgType.Float){           //数字框
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
                        }
                    }
                }
                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
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
        }
    }

    Component{
        id:componentComboBox
        RowLayout{
            spacing: 5
            width: parameterList.width
            property var value: null
            ListModel{
                id: comboModel
            }
            Text{
                id:nametext
                text: model.name
            }
            Rectangle{
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                color: "black"
            }
            ComboBox{
                id:parameterComboBox
                model: comboModel
                onCurrentIndexChanged: {
                    value = currentIndex
                    root.setParam(index, value)
                }
            }

            Component.onCompleted: {
                if(model && model.content){
                    let parts = model.content.split("|")
                    let items = parts[0].split(",")
                    comboModel.clear()
                    for(let i=0; i<items.length; i++){
                        comboModel.append({"text":items[i]})
                    }
                    let defaultIndex = parts.length > 1 ? parseInt(parts[1]) : 0
                    if (isNaN(defaultIndex) || defaultIndex < 0 || defaultIndex >= items.length) {
                        defaultIndex = 0
                    }
                    parameterComboBox.currentIndex = defaultIndex
                    value = defaultIndex
                    root.setParam(index, value)
                }
                value = parameterComboBox.currentIndex
                root.setParam(index, value)
            }
        }
    }
    Component{
        id:oneNumberBox
        RowLayout{
            spacing: 5
            width: parameterList.width
            Text{
                id:nametext
                text: model.name
            }
            Rectangle{
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                color: "black"
            }
            TextField {
                id:parameterTextInput
                Layout.fillWidth: parent.width
                text: model.content
                onTextChanged:{
                    root.setParam(index, parseFloat(text))
                }
            }
        }
    }
    Component{
        id:fileComponent
        RowLayout{
            spacing: 5
            width: parameterList.width
            Text{
                id:nametext
                text: model.name
            }
            Rectangle{
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                color: "black"
            }
            TextArea{
                id:fileText
                wrapMode: TextEdit.Wrap
                Layout.fillWidth: parent.width

                Component.onCompleted: {
                    fileText.text = model.content
                    root.setParam(index, fileText.text)
                }
                onEditingFinished: {
                    root.setParam(index, fileText.text)
                }
            }
            Button{
                text: "...."
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
        id:textComponent
        RowLayout{
            spacing: 5
            width: parameterList.width
            property var value: fileText.text
            Text{
                id:nametext
                text: model.name
                Layout.preferredWidth: 120
                elide: Text.ElideRight
                ToolTip.visible: nameHover.hovered && model.description.length > 0
                ToolTip.text: model.description
                HoverHandler {
                    id: nameHover
                }
            }
            Rectangle{
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                color: "black"
            }
            TextArea{
                id:fileText
                wrapMode: TextEdit.Wrap
                Layout.fillWidth: true
                placeholderText: model.description
                ToolTip.visible: hovered && model.description.length > 0
                ToolTip.text: model.description

                Component.onCompleted: {
                    fileText.text = model.content
                    root.setParam(index, fileText.text)
                }
                onEditingFinished: {
                    root.setParam(index, fileText.text)
                }
            }
        }
    }
    Component{
        id: selectorComponent
        RowLayout{
            spacing: 5
            width: parameterList.width
            property var value: null

            Text{
                id:nametext
                text: model.name
            }
            Rectangle{
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                color: "black"
            }
            Text{
                id:selectedItems
                text: value ? value.size():"无"
            }

            Button{
                id: selectStartButton
                text: "开始选择"
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
                function onConfirmed(selection) {
                    value = selection
                    root.setParam(index, value)
                    App.selection.listeningSelectorIndex = -1
                }
            }

            Connections {
                target: App.selection
                function onSelectionInvalidated() {
                    value = null
                    root.setParam(index, null)
                    if (App.selection.listeningSelectorIndex === index)
                        App.selection.listeningSelectorIndex = -1
                }
            }
        }
    }
    Component{
        id: boolComponent
        RowLayout{
            spacing: 5
            width: parameterList.width

            Text{
                id:nametext
                text: model.name
            }
            Rectangle{
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                color: "black"
            }
            CheckBox{
                id: parameterCheckBox

                Component.onCompleted: {
                    checked = (model.content === "true")
                    root.setParam(index, checked)
                }
                onCheckedChanged: {
                    root.setParam(index, checked)
                }
            }
        }
    }
}
