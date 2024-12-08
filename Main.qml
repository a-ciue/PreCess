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
    title: qsTr("Hello World")

    header: ToolBar {
        id: header
        RowLayout {
            anchors.fill: parent

            FileDialog {
                id: splineDialog
                nameFilters: ["STP File (*.stp)"]
                onAccepted: {
                    myItem.readSpline(selectedFile);
                }
            }
            FileDialog {
                id: saveDialog
                nameFilters: ["OBJ File (*.obj)"]
                fileMode: FileDialog.SaveFile
                onAccepted: {
                    myItem.writeMesh(selectedFile);
                }
            }

            RowLayout {
                spacing: 3
                ToolButton{
                    id:fileButton
                    text:"File"
                    Layout.preferredWidth: 40
                    onClicked:menu.open()
                    Menu{
                        id:menu
                        closePolicy: Popup.CloseOnPressOutsideParent
                        MenuItem{
                            text: "指定样条剖分"
                            onClicked: splineDialog.open()
                        }
                        MenuItem{
                            text: "导出网格"
                            onClicked: saveDialog.open()
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
                        console.log("renderGroup checkedButton changed")
                        myItem.changeRenderer(checkedButton.renderMode);
                    }
                }
                Button {
                    id: btn1
                    text: "Face"
                    property string renderMode: "Face"
                    rightPadding: 8
                    checkable: true
                    checked: true
                    onClicked: stacklayout.currentIndex = 0
                    ButtonGroup.group: renderGroup
                }
                Button {
                    id: btn2
                    text: "Block"
                    property string renderMode: "Block"
                    checkable: true
                    onClicked: stacklayout.currentIndex = 1
                    ButtonGroup.group: renderGroup
                }
                Button {
                    id: btn3
                    text: "Group"
                    property string renderMode: "Group"
                    checkable: true
                    onClicked: stacklayout.currentIndex = 2
                    ButtonGroup.group: renderGroup
                }

                Component.onCompleted: {
                    let width = Math.max(btn1.width, btn2.width, btn3.width)
                    btn1.Layout.preferredWidth = width
                    btn2.Layout.preferredWidth = width
                    btn3.Layout.preferredWidth = width
                }
            }
            //      Item {
            //        Layout.fillWidth: true
            //        Layout.fillHeight: true
            //      }
            //      Rectangle {
            //        color: "black"
            //        Layout.preferredWidth: 1
            //        Layout.fillHeight: true
            //      }
            //      Text {
            //        Layout.leftMargin: 10
            //        text: "vtkSource:"
            //      }
            //      ComboBox {
            //        id: sources
            //        Layout.fillHeight: true
            //        Layout.preferredWidth: childrenRect.width
            //        model: Presenter.sources
            //      }
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

        Item{
            id:face_mode
            anchors.fill: parent
            ButtonGroup{
                buttons:row1.children
            }
            RowLayout{
                id:row1
                anchors.fill: parent
                spacing: 3
                ButtonGroup {
                    id: face_group
                    onCheckStateChanged: {
                        if(checkState == Qt.Unchecked)
                            myItem.unbindStyle();
                    }
                }

                Button{
                    id:edgeCutButton
                    text: "Cut Edge"
                    ButtonGroup.group: face_group
                    onClicked: toggle()
                    onCheckedChanged: if (checked) myItem.bindStyle("Edge")
                }
                Button{
                    id:faceCutButton
                    text: "Cut Face"
                    ButtonGroup.group: face_group
                    onClicked: toggle()
                    onCheckedChanged: {
                        if (checked) myItem.bindStyle("Face")
                    }
                }
                Item {
                  Layout.fillWidth: true
                  Layout.fillHeight: true
                }
                Button{
                    id:edgeCutConfirm
                    text: "confirm"
                    Layout.alignment: Qt.AlignRight
                    visible:edgeCutButton.checked
                    onClicked:{
                        myItem.commitEdgeCut()
                        edgeCutButton.toggle()
                    }
                }
                Button{
                    id:faceCutConfirm
                    text: "confirm"
                    Layout.alignment: Qt.AlignRight
                    visible:faceCutButton.checked
                    onClicked:{
                        myItem.commitFaceCut()
                        faceCutButton.toggle()
                    }
                }
            }
        }
        Item{
            id:patch_mode
            anchors.fill: parent
            RowLayout{
                anchors.fill: parent
                Button{
                    id:integrateBlockButton
                    text: "Integrate Block"
                    onClicked:{
                        toggle()
                    }
                    onCheckedChanged: {
                        if (checked) myItem.bindStyle("Block");
                        else myItem.unbindStyle();
                    }
                }
                Button{
                    id:patchMergeConfirm
                    text: "合并"
                    visible:integrateBlockButton.checked
                    Layout.alignment: Qt.AlignRight
                    onClicked:{
                        myItem.commitBlockMerge()
                        integrateBlockButton.toggle()
                    }
                }
                Button{
                    id:patchRemeshConfirm
                    text: "重网格"
                    visible:integrateBlockButton.checked
                    Layout.alignment: Qt.AlignRight
                    onClicked:{
                        myItem.commitBlockRemesh()
                        integrateBlockButton.toggle()
                    }
                }
            }
        }
        Item{
            id:group_mode
            anchors.fill: parent
            RowLayout{
                anchors.fill: parent
                Button{
                    id:integrateGroupButton
                    text:qsTr("Integrate Group")
                    onClicked:{
                        toggle()
                    }
                    onCheckedChanged: {
                        if (checked) myItem.bindStyle("Group");
                        else myItem.unbindStyle();
                    }
                }
                Button{
                    id:groupModeConfirm
                    text: "合并"
                    visible:integrateGroupButton.checked
                    Layout.alignment: Qt.AlignRight
                    onClicked:{
                        myItem.commitGroupMerge()
                        integrateGroupButton.toggle()
                    }
                }
                Button{
                    id:groupRemeshConfirm
                    text: "重网格"
                    visible:integrateGroupButton.checked
                    Layout.alignment: Qt.AlignRight
                    onClicked:{
                        myItem.commitGroupRemesh()
                        integrateGroupButton.toggle()
                    }
                }
            }
        }
    }



    Rectangle {
        //anchors.top:stacklayout.bottom
        anchors.bottom:parent.bottom
        anchors.top:stacklayout.bottom
        anchors.left:parent.left
        anchors.right:parent.right
        //Layout.fillWidth:true
        //height: 600
        border {
            id: border
            width: 5;
            color: dsv.focused === item ? "goldenrod" : "steelblue"
        }
        radius: 5
        color: "magenta"
        //anchors.fill: parent

        MyVtkItem {
            id: myItem
            anchors.fill: parent
            anchors.margins: border.width
            //        source: sources.currentText
        }
    }
}
