/**
 * @file QSideBar.qml
 * @brief 侧边栏，执行复杂算法时提供参数的交互界面
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore

import app.core
import app.model
import app.model.systems.algo

Item{
    id: root
    property var system
    property var curAlgoInfo
    property int curComponent
    required property SignalListener confirm_listener
    property var parameters: []
    signal selectModeChanged
    signal cancleCommand

    Settings {
        id: parameterSettings
        category: "AlgorithmParameters"
        property string values: "{}"
    }

    function parameterSettingsObject() {
        try {
            return JSON.parse(parameterSettings.values || "{}")
        } catch (e) {
            return {}
        }
    }

    function parameterKey(index, argType) {
        let algorithmName = curAlgoInfo ? curAlgoInfo.name : ""
        let parameterName = argType && argType.name ? argType.name : index
        return algorithmName + "::" + index + "::" + parameterName
    }

    function loadParameter(index, argType, defaultValue) {
        let values = parameterSettingsObject()
        let key = parameterKey(index, argType)
        return Object.prototype.hasOwnProperty.call(values, key) ? values[key] : defaultValue
    }

    function saveParameter(index, argType, value) {
        if (!curAlgoInfo || value === undefined || value === null) {
            return
        }
        let values = parameterSettingsObject()
        values[parameterKey(index, argType)] = value
        parameterSettings.values = JSON.stringify(values)
    }

    onCurAlgoInfoChanged: {
        parameters = []
        parameterList.model = curAlgoInfo.arg_types
    }
    Button{
        id: commitButton
        text: "执行"
        enabled: curComponent >= 0 && curAlgoInfo != null
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height:30
        onClicked:{
            system.call(curAlgoInfo.name, curComponent, root.parameters)
        }
    }
    Item{
        anchors.top: commitButton.bottom
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
    /*Component{
        id:componentButton
        Row{
            property alias exposedText: textbar.text   //name
            //加入新属性value
            function commitInformation(){
                console.log(information.text)
            }

            Text{
                id:textbar
            }
            Button{
                id:information
                text: "text_button"
            }
        }
    }*/
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
                    root.parameters[index] = value = currentIndex
                    root.saveParameter(index, model, currentIndex)
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
                    let savedIndex = root.loadParameter(index, model, defaultIndex)
                    savedIndex = parseInt(savedIndex)
                    if (isNaN(savedIndex) || savedIndex < 0 || savedIndex >= items.length) {
                        savedIndex = defaultIndex
                    }
                    parameterComboBox.currentIndex = savedIndex
                    root.parameters[index] = value = savedIndex
                }
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
                text: root.loadParameter(index, model, model.content)
                Component.onCompleted: {
                    let value = parseFloat(text)
                    root.parameters[index] = value
                    if (!isNaN(value)) {
                        root.saveParameter(index, model, value)
                    }
                }
                onTextChanged:{
                    let value = parseFloat(text)
                    root.parameters[index] = value
                    if (!isNaN(value)) {
                        root.saveParameter(index, model, value)
                    }
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
                    fileText.text = root.loadParameter(index, model, model.content)
                    root.parameters[index] = fileText.text
                }
                onEditingFinished: {
                    root.parameters[index] = fileText.text
                    root.saveParameter(index, model, fileText.text)
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
                    root.parameters[index] = fileText.text
                    root.saveParameter(index, model, fileText.text)
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
                
                Component.onCompleted: {
                    fileText.text = root.loadParameter(index, model, model.content)
                    root.parameters[index] = fileText.text
                }
                onEditingFinished: {
                    root.parameters[index] = fileText.text
                    root.saveParameter(index, model, fileText.text)
                }
            }
        }
    }
    Component{
        id: selectorComponent
        RowLayout{
            // id: root
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
                enabled: root.curComponent >= 0
                onClicked:{
                    root.selectModeChanged()
                    checked = !checked
                }
                onCheckedChanged: {
                    if (checked) {
                        root.confirm_listener.registerSignalListener(changeSelectionOnce)
                    } else {
                        root.confirm_listener.unregisterSignalListener(changeSelectionOnce)
                    }
                }

                function changeSelectionOnce(selection) {
                    root.parameters[index] = value = selection
                    checked = false
                }
            }
        }
    }
}
