/**
 * @file QSideBar.qml
 * @brief 侧边栏，执行复杂算法时提供参数的交互界面
 */

import QtQuick 2.15
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs 6.3

Item{
    id: root
    signal selectModeChanged
    Button{
        id: commitButton
        text: "commit"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height:30
        onClicked:{
            for(var i in parameterList.loadedItems){
                console.log(parameterList.loadedItems[i].value)
            }
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
            ListView{              //model数据层 delegate视图层
                id:parameterList
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.margins: 3
                spacing: 5
                property var loadedItems: ({})
                delegate:Loader{
                    sourceComponent:{
                        if(type === 0){           //文件
                            return fileComponent
                        }
                        if(type === 1){           //多选一
                            return componentComboBox
                        }
                        if(type === 2){           //数字框
                            return oneNumberBox
                        }
                        if(type === 3){           //选择器
                            return selectorComponent
                        }
                    }
                    Component.onCompleted: {
                        if(item){
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
                                parameterList.loadedItems[index].parameter1 = content
                            }
                        }
                    }
                    Component.onDestruction: {
                        delete parameterList.loadedItems[index]
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
    Component{
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
    }
    Component{
        id:fileComponent
        RowLayout{
            spacing: 5
            property alias name: nametext.text
            property alias parameter1: fileText.text
            property var value: fileText.text
            Text{
                id:nametext
            }
            Rectangle{
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                color: "black"
            }
            TextArea{
                id:fileText
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
        id: selectorComponent
        RowLayout{
            // id: root
            spacing: 5
            /** type:string */
            property alias name: nametext.text
            property alias parameter1: selectedItems.text
            property var value: selectedItems.text

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
            }

            Button{
                id: selectStartButton
                text: "开始选择"
                onClicked:{
                    //root.selectModeChanged()
                }
            }

        }
    }
    /** type:var 侧边栏的model数据构造 */
    property alias m: parameterList.model       //对main.qml的属性接口
}
