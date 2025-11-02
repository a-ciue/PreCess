/**
 * @file QSideBar.qml
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
    property var system
    property var curAlgoInfo
    property int curModel
    property var savedSelection: []
    required property QSelection curSelection // temp
    property var parameters: []
    signal selectModeChanged
    signal cancleCommand

    onCurAlgoInfoChanged: {
        parameters = []
        parameterList.model = curAlgoInfo.arg_types
    }
    Button{
        id: commitButton
        text: "执行"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height:30
        onClicked:{
            system.call(curAlgoInfo.name, curModel, root.parameters)
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
                property var loadedItems: ({})
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
                        Component.onCompleted: {
                        /*if(item){
                            parameterList.loadedItems[index] = item
                            if(type === 0){
                                parameterList.loadedItems[index].name = name
                                parameterList.loadedItems[index].parameter1 = content
                            }
                            if(type === 1){
                                parameterList.loadedItems[index].name = name
                                parameterList.loadedItems[index].parameter1 = content
                            }
                            if(type === 2){
                                parameterList.loadedItems[index].name = name
                                parameterList.loadedItems[index].parameter1 = content
                            }
                            if(type === 3){
                                parameterList.loadedItems[index].name = name
                                //parameterList.loadedItems[index].parameter1 = content
                            }
                        }*/
                        }
                        Component.onDestruction: {
                            delete parameterList.loadedItems[index]
                        }
                    }
                }
            }
        }
        Rectangle{
            id: scrollbar
            anchors.right: parent.right
            y: parameterList.visibleArea.yPosition * parameterList.height
            width: 5
            height: parameterList.visibleArea.heightRatio * parameterList.height
            color: "grey"
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
                } 
            }

            Component.onCompleted: {
                if(model && model.content){
                    let items = model.content.split(",")
                    comboModel.clear()
                    for(let i=0; i<items.length; i++){
                        comboModel.append({"text":items[i]})
                    }
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
                onTextChanged:{
                    root.parameters[index] = parseFloat(text)
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
                    root.parameters[index] = fileText.text
                }
                onEditingFinished: {
                    root.parameters[index] = fileText.text
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
                    fileText.text = model.content
                    root.parameters[index] = fileText.text
                }
                onEditingFinished: {
                    root.parameters[index] = fileText.text
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
                onClicked:{
                    root.selectModeChanged()
                    checked = !checked
                }
                onCheckedChanged: {
                    if (checked) {
                        root.curSelectionChanged.connect(changeSelectionOnce)
                    } else {
                        root.curSelectionChanged.disconnect(changeSelectionOnce)
                    }
                }

                function changeSelectionOnce() {
                    root.parameters[index] = value = root.curSelection
                    checked = false
                }
            }
        }
    }

    function updateSelectorCount(selectorIndex, selectType) {
        console.log("尝试更新选择器计数，索引:", selectorIndex)
        console.log("当前加载的组件:", parameterList.loadedItems)
        
        // 获取对应索引的选择器组件
        var selector = parameterList.loadedItems[selectorIndex]
        console.log("获取到的选择器:", selector)
        
        if (selector) {
            if (selectType === "边") {
                selector.valueEdge++
                console.log("边计数更新为:", selector.valueEdge)
            } else if (selectType === "面") {
                selector.valueFace++
                console.log("面计数更新为:", selector.valueFace)
            } else if (selectType === "块") {
                selector.valueBlock++
                console.log("块计数更新为:", selector.valueBlock)
            }
        } else {
            console.log("未找到选择器组件，请检查索引是否正确")
        }
    }

    function clearSelectorCount(selectorIndex) {
        // 获取对应索引的选择器组件
        var selector = parameterList.loadedItems[selectorIndex]
        if (selector) {
            selector.valueEdge = 0
            selector.valueFace = 0
            selector.valueBlock = 0
        }
    }
    property alias m: parameterList.model
}
