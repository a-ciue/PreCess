import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs
import QtQuick.Controls 6.7


ApplicationWindow {
    visible: true
    width: 640
    height: 640
    title: qsTr("三角剖分交互程序")

    required property ModelManager modelManager

    header: ToolBar {
        id: header
        RowLayout {
            anchors.fill: parent

            FileDialog {
                id: splineDialog
                nameFilters: ["STP File (*.stp)"]
                onAccepted: {
                    modelManager.readSpline(selectedFile);
                }
            }
            FileDialog {
                id: openPatchDialog
                nameFilters: ["OBJ File (*.obj)"]
                onAccepted: {
                    modelManager.readMesh(selectedFile);
                }
            }
            FileDialog {
                id: saveFaceDialog
                nameFilters: ["OBJ File (*.obj)", "INP File (*.inp)"]
                fileMode: FileDialog.SaveFile
                onAccepted: {
                    modelManager.writeMesh(selectedFile, "Face", selectedNameFilter.extensions[0]);
                }
            }
            FileDialog {
                id: saveBlockDialog
                nameFilters: ["OBJ File (*.obj)", "INP File (*.inp)"]
                fileMode: FileDialog.SaveFile
                onAccepted: {
                    modelManager.writeMesh(selectedFile, "Block", selectedNameFilter.extensions[0]);
                }
            }
            FileDialog {
                id: saveGroupDialog
                nameFilters: ["OBJ File (*.obj)", "INP File (*.inp)"]
                fileMode: FileDialog.SaveFile
                onAccepted: {
                    modelManager.writeMesh(selectedFile, "Group", selectedNameFilter.extensions[0]);
                }
            }

            RowLayout {
                spacing: 3
                ToolButton{
                    id:fileButton
                    text:"文件"
                    Layout.preferredWidth: 40
                    onClicked:menu.open()
                    Menu{
                        id:menu
                        closePolicy: Popup.CloseOnPressOutsideParent
                        MenuItem{
                            text: "指定样条剖分"
                            onClicked: splineDialog.open()
                        }

                        MenuItem {
                            text: "导入"
                            onClicked: openPatchDialog.open()
                        }

                        Menu {
                            title: "导出..."
                            MenuItem{
                                text: "网格"
                                onClicked: saveFaceDialog.open()
                            }
                            MenuItem{
                                text: "网格（带分块信息）"
                                onClicked: saveBlockDialog.open()
                            }
                            MenuItem{
                                text: "网格（带分组信息）"
                                onClicked: saveGroupDialog.open()
                            }
                        }
                    }
                }
                Rectangle {
                    color: "black"
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                }

                ButtonGroup {
                    id: renderGroup
                    onCheckedButtonChanged: {
                        myItem.unbindStyle()
                        myItem.changeRenderer(checkedButton.renderMode)
                    }
                }
                Button {
                    id: btn1
                    text: "面模式"
                    property string renderMode: "Face"
                    rightPadding: 8
                    checkable: true
                    checked: true
                    onClicked: stacklayout.currentIndex = 0
                    ButtonGroup.group: renderGroup
                }
                Button {
                    id: btn2
                    text: "块模式"
                    property string renderMode: "Block"
                    checkable: true
                    onClicked: stacklayout.currentIndex = 1
                    ButtonGroup.group: renderGroup
                }
                Button {
                    id: btn3
                    text: "组模式"
                    property string renderMode: "Group"
                    checkable: true
                    onClicked: stacklayout.currentIndex = 2
                    ButtonGroup.group: renderGroup
                }

                Component.onCompleted: {
                    let width = 1.5*Math.max(btn1.width, btn2.width, btn3.width)
                    btn1.Layout.preferredWidth = width
                    btn2.Layout.preferredWidth = width
                    btn3.Layout.preferredWidth = width
                }
            }
        }
    }

    Item{
        id:index
        property bool trigger: false
    }

    StackLayout{
        id:stacklayout
        anchors.left: parent.left
        anchors.right: parent.right
        height: 25

        SelectingBar{
            id:facemode
            Layout.fillWidth: true
            Layout.fillHeight: true
            modeButtonModel:ListModel{
                ListElement{
                    name:"切分边"
                }
                ListElement{
                    name:"切分面"
                }
            }
            confirmButtonModel:ListModel{
                ListElement{
                    name:"确认"
                }
                ListElement{
                    name:"确认"
                }
            }
            onButtonFunction:{
                if(modeOrConfirm === 0){
                    if(index === 0){
                        myItem.bindStyle("Edge")
                    }
                    if(index === 1){
                        myItem.bindStyle("Face")
                    }
                }else{
                    if(index === 0){
                        myItem.commitEdgeCut()
                    }
                    if(index === 1){
                        myItem.commitFaceCut()
                    }
                }
            }
            onButtonGroupFunction:{
                myItem.unbindStyle()
            }
            onChangeEdgeRender:{
                myItem.changeEdgeRender("Face", check)
            }
        }

        SelectingBar{
            id:patchmode
            Layout.fillWidth: true
            Layout.fillHeight: true
            modeButtonModel:ListModel{
                ListElement{
                    name:"块合并"
                }
                ListElement{
                    name:"块重网格"
                }
            }
            confirmButtonModel:ListModel{
                ListElement{
                    name:"确认"
                }
                ListElement{
                    name:"确认"
                }
            }
            onButtonFunction:{
                if(modeOrConfirm === 0){
                    if(index === 0) { myItem.bindStyle("Block")}
                    else { myItem.bindStyle("Block")}
                }else{
                    if(index === 0) { myItem.commitBlockMerge()}
                    else { myItem.commitGroupRemesh()}
                }
            }
            onButtonGroupFunction:{
                myItem.unbindStyle()
            }
            onChangeEdgeRender:{
                myItem.changeEdgeRender("Block", check)
            }
        }


        SelectingBar{
            id:groupmode
            Layout.fillWidth: true
            Layout.fillHeight: true
            modeButtonModel:ListModel{
                ListElement{
                    name:"组合并"
                }
                ListElement{
                    name:"组重网格"
                }
            }
            confirmButtonModel:ListModel{
                ListElement{
                    name:"确认"
                }
                ListElement{
                    name:"确认"
                }
            }
            onButtonFunction:{
                if(modeOrConfirm === 0){
                    if(index === 0) { myItem.bindStyle("Group")}
                    else{ myItem.bindStyle("Group")}
                }else{
                    if(index === 0) { myItem.commitGroupMerge()}
                    else{ myItem.commitGroupRemesh()}
                }
            }
            onButtonGroupFunction:{
                myItem.unbindStyle()
            }
            onChangeEdgeRender:{
                myItem.changeEdgeRender("Group", check)
            }
        }
    }

    Rectangle {
        anchors.bottom:parent.bottom
        anchors.top:stacklayout.bottom
        anchors.left:parent.left
        anchors.right:parent.right
        border {
            id: border
            width: 5
        }
        radius: 5
        color: "magenta"

        MyVtkItem {
            id: myItem
            anchors.fill: parent
            anchors.margins: border.width
        }
        Component.onCompleted: modelManager.vtkItem = myItem
    }
}
