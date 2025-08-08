/**
 * @file QSideBar.qml
 * @brief 侧边栏，执行复杂算法时提供参数的交互界面
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import app
import app.core
import app.model
import app.model.systems.algo

Item{
    id: root
    required property QAlgorithmSystemAdaptor algorithmSystem
    property QAlgorithmInfo curAlgoInfo
    property int curModel
    property var savedSelection: []
    required property QSelection curSelection // temp
    property Item paraList: parameterList
    signal selectModeChanged
    signal cancleCommand

    onCurAlgoInfoChanged: {
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
            //let params = parameterList.model.map(model => model.value)

            //commandDispatcher.runCommand(curCommand, curModel, 
            algorithmSystem.call(curAlgoInfo.name, curModel, parameterList.model)
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
                        required property int type  // ArgTypeEnum
                        required property string name
                        required property string content
                        required property string description
                        required property var value
                        sourceComponent:{
                            if(curAlgoInfo && curAlgoInfo.name === "切分边"){
                                return splictEdgeComponent
                            }
                            if(curAlgoInfo && curAlgoInfo.name === "切分面"){
                                return splictFaceComponent
                            }

                            if(type === QArgType.Path){           //文件
                                return fileComponent
                            }
                            if(type === QArgType.Combo){           //多选一
                                return componentComboBox
                            }
                            if(type === QArgType.Float){           //数字框
                                return oneNumberBox
                            }
                            if(type === QArgType.Selector){           //选择器
                                return selectorComponent
                            }
                            if(type === QArgType.Text){           //文字输入框
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
    }
    Component{
        id:componentComboBox
        RowLayout{
            spacing: 5
            property alias name: nametext.text
            property alias parameter1: parameterComboBox.currentIndex
            property var value: parameterComboBox.currentText
            // function commitInformation(){
            //     console.log(information.currentText)
            // }
            Text{
                id:nametext
            }
            Rectangle{
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                color: "black"
            }
            ComboBox{
                id:parameterComboBox
                model:ListModel{
                    ListElement{text:"方法A"}
                    ListElement{text:"方法B"}
                }
            }
        }
    }
    Component{
        id:oneNumberBox
        RowLayout{
            spacing: 5
            property alias name: nametext.text
            property alias parameter1: parameterTextInput.text
            property var value: parameterTextInput.text
            Text{
                id:nametext
            }
            Rectangle{
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                color: "black"
            }
            TextInput{
                id:parameterTextInput
                Layout.fillWidth: true
                Layout.fillHeight: true
                horizontalAlignment: TextInput.AlignHCenter
                //verticalAlignment: TextInput.AlignVCenter
                onTextChanged:{
                    console.log(height,width,"TextInput的height和width")
                }
            }
        }
    }*/
    Component{
        id:fileComponent
        RowLayout{
            spacing: 5
            width: parameterList.width
            property alias value: fileText.text
            Text{
                id:nametext
                text: name
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
                    fileText.text = content
                    model.value = fileText.text
                }
                onEditingFinished: {
                    model.value = fileText.text
                }
            }
            Button{
                text: "打开文件"
                onClicked:{
                    parameterFileDialog.open()
                }
            }
            FileDialog{
                id:parameterFileDialog
                onAccepted:{
                    fileText.text = selectedFile
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
                text: name
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
                    fileText.text = content
                    model.value = fileText.text
                }
                onEditingFinished: {
                    model.value = fileText.text
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
            /** type:string */
            property int valueEdge : 0
            property int valueFace : 0
            property int valueBlock : 0
            property var selected
            property int type: 3  // 添加类型标识

            Text{
                id:nametext
            }
            Rectangle{
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                color: "black"
            }
            Text{
                id:selectedItems
                text: valueEdge + "边" + valueFace + "面" + valueBlock + "块"
            }

            Button{
                id: selectStartButton
                text: "开始选择"
                onClicked:{
                    root.selectModeChanged()
                }
            }
            property alias name: nametext.text
            property alias parameter1: selectedItems.text
            //property alias selectedItems: selected
        }
    }
    Component{
        id:splictEdgeComponent
        RowLayout{
            property int value: 0
            width: parameterList.width
            Text{
                text:"已选择"+value+"条边"
            }
            Button{
                Layout.preferredWidth: 40
                text: "取消"
                onClicked: {
                    cancleCommand()
                }
            }
            function addSelection(){
                if(!value){
                    value = 1
                }
            }
        }
    }
    Component{
        id:splictFaceComponent
        RowLayout{
            property int value: 0
            width: parameterList.width
            Text{
                text:"已选择"+value+"个面"
            }
            Button{
                Layout.preferredWidth: 40
                text: "取消"
                onClicked: {
                    cancleCommand()
                }
            }
            function addSelection(){
                if(!value){
                    value = 1
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
