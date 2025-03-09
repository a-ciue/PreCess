import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs
import QtQuick.Controls 6.7

import fileLoader

ApplicationWindow {
    visible: true
    width: 800
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
            /*confirmButtonModel:ListModel{
                ListElement{
                    name:"确认"
                }
                ListElement{
                    name:"确认"
                }
            }*/
            onButtonFunction:{
                if(modeOrConfirm === 0){
                    if(index === 0){
                        // myItem.bindStyle("Edge")
                        let ids = myItem.selectedIDs;
                        //if(ids.type() !== Element.Edge){console.log("ids的类型不是Element.Edge")}
                        console.log(ids.type())
                        if (ids.size() !== 0 /*&& ids.type() === Element.Edge*/) {
                            modelManager.model(ids.getName()).split_edge(ids.ids(0), ids.ids(1), ids.ids(2))
                        }else{
                            console.log("未选中对象或选中对象不是边")
                        }

                        selector.clearSelection()
                    }
                    if(index === 1){
                        // myItem.bindStyle("Face")
                        let ids = myItem.selectedIDs;
                        if (ids.size() !== 0 /*&& ids.type() === Element.Face*/) {
                            modelManager.model(ids.getName()).split_face(ids.ids(0), ids.ids(1))
                        }else{
                            console.log("未选中对象或选中对象不是面")
                        }

                        selector.clearSelection()
                    }
                }else{
                    if(index === 0){
                        let ids = myItem.selectedIDs;
                        if (ids.size() !== 0) {
                            modelManager.model("a").split_edge(ids.ids(0), ids.ids(1), ids.ids(2))
                        }
                    }
                    if(index === 1){
                        let ids = myItem.selectedIDs;
                        if (ids.size() !== 0) {
                            modelManager.model("a").split_face(ids.ids(0), ids.ids(1))
                        }
                    }
                }
            }
            onButtonGroupFunction:{
                myItem.unbindStyle()
            }
            onChangeEdgeRender:{
                myItem.changeEdgeRender(myItem.selectedIDs.getName(),"Face", check)
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
            /*confirmButtonModel:ListModel{
                ListElement{
                    name:"确认"
                }
                ListElement{
                    name:"确认"
                }
            }*/
            onButtonFunction:{
                if(modeOrConfirm === 0){
                    if(index === 0) {
                        // myItem.bindStyle("Block")
                        modelManager.model("a").merge_blocks(myItem.selectedIDs.data.ids)
                        myItem.resetCamera()
                        selector.clearSelection()
                    }
                    else {
                        // myItem.bindStyle("Block")
                        modelManager.model("a").remesh_block(myItem.selectedIDs.data.ids)
                        myItem.resetCamera()
                        selector.clearSelection()
                    }
                }else{
                    if(index === 0) 
                    { 
                        modelManager.model("a").merge_blocks(myItem.selectedIDs.data.ids)
                        myItem.resetCamera()
                    }
                    else { 
                        modelManager.model("a").remesh_block(myItem.selectedIDs.data.ids)
                        myItem.resetCamera()
                    }
                }
            }
            onButtonGroupFunction:{
                myItem.unbindStyle()
            }
            onChangeEdgeRender:{
                myItem.changeEdgeRender("a","Block", check)
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
            /*confirmButtonModel:ListModel{
                ListElement{
                    name:"确认"
                }
                ListElement{
                    name:"确认"
                }
            }*/
            onButtonFunction:{
                if(modeOrConfirm === 0){
                    if(index === 0){ 
                        // myItem.bindStyle("Group")
                        modelManager.model("a").merge_groups(myItem.selectedIDs.data().ids)
                        myItem.resetCamera()
                        selector.clearSelection()
                    }
                    else{
                        // myItem.bindStyle("Group")
                        modelManager.model("a").remesh_group(myItem.selectedIDs.data().ids)
                        myItem.resetCamera()
                        myItem.unbindStyle()
                        selector.clearSelection()
                    }
                }else{
                    if(index === 0){
                        modelManager.model("a").merge_groups(myItem.selectedIDs.data.ids)
                        myItem.resetCamera()
                    }
                    else{
                        modelManager.model("a").remesh_group(myItem.selectedIDs.data.ids)
                        myItem.resetCamera()
                    }
                }
            }
            onButtonGroupFunction:{
                myItem.unbindStyle()
            }
            onChangeEdgeRender:{
                myItem.changeEdgeRender("a","Group", check)
            }
        }
    }

    Rectangle{
        id:devider
        height:1
        color: "black"
        anchors.top: stacklayout.bottom
        anchors.left: parent.left
        anchors.right: myItemRectangle.left
    }

    ObjectList{
        id:objectList
        anchors.top: devider.bottom
        anchors.left: parent.left
        anchors.right: myItemRectangle.left
        //anchors.bottom: sideBar.top
        width: 160
        height: 200
        objectModel:ListModel{
            id: objectInitializeModel
            ListElement{name: "对象甲"}
            ListElement{name: "对象乙"}
            ListElement{name: "对象丙"}
            ListElement{name: "对象丁"}
        }
        onButtonPressed:{
            if(type === 1){
                /*此处是点击对象列表“隐藏”按钮后执行的函数*/
                //console.log("对象"+index+"已被隐藏")
            }
            if(type === 2){
                /*此处是电机对象列表“删除”按钮后执行的函数*/
                //console.log("对象"+index+"已被删除")
                //objectInitializeModel.remove(index,1)
            }
        }
        Component.onCompleted: {
            modelManager.modelAdded.connect(objectList.addItem)
            modelManager.modelRemoved.connect((modelName)=>{objectList.removeItem(modelName)})
            modelManager.modelNameChanged.connect((modelName)=>{objectList.renameItem(modelName)})
            objectList.removeModel.connect((modelName)=>{modelManager.removeModel(modelName)})
            objectList.changeModelVisibility.connect(myItem.setVisibility)
        }
    }

    SideBar{
        id: sideBar
        anchors.top: objectList.bottom
        anchors.left: parent.left
        anchors.right: myItemRectangle.left
        anchors.bottom: parent.bottom
        width: 160
        m:ListModel{
            ListElement{type: 2; name: "属性甲"; content: "55"}
            ListElement{type: 2; name: "属性乙"; content: "43"}
            ListElement{type: 1; name: "属性丙"; content: 1}
            ListElement{type: 0; name: "属性丁"; content: "无"}
            ListElement{type: 3; name: "选择器"; content: "无"}//可能会有多个选择器的需求，因此需要动态构造多个选择器
        }
        onSelectModeChanged:{         //应该加上参数以判断是哪一个选择器选择的对象
            selector.changePropertyEnabled()
            selector.comboBoxSelectionChanged()
            //console.log("信号被接收")
        }
        // Component.onCompleted: {
        //     selector.comboBoxSelectionChanged()
        // }
    }

    Rectangle {
        id: myItemRectangle
        anchors.bottom:parent.bottom
        anchors.top:stacklayout.bottom
        anchors.left:objectList.right
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
        Component.onCompleted: {
            modelManager.vtkItem = myItem
            modelManager.modelRemoved.connect((modelName)=>
            myItem.deleteModel(modelName))
        }

        Selector{
            id:selector
            anchors.top:  myItem.top
            anchors.left: myItem.left
            anchors.topMargin: 10
            anchors.leftMargin: 10
            property var selector_ids
            onSelectorButtonClicked:{
                if(type === 0){
                    console.log("点击清除按钮")
                    /*此处是点击选择器清除按钮之后执行的函数*/
                }
                if(type === 1){
                    console.log("点击确认按钮")
                    selection = myItem.selectedIDs;
                    /*此处是点击选择器的确认按钮之后执行的函数*/
                    /*下面是我写的测试函数，可删，Selection.h中的initialize()函数是我赋值用的，可删，ids(i)是输出第i个id的，或许有用*/
                    /*selection.initialize()
                    var i
                    for(i = 0;i<selection.size();i++){
                        console.log(selection.ids(i))
                    }

                    if(selection.type() === Element.Face){
                        console.log("Face已被接收到")
                    }*/            
                }
            }

            function clearSelection(){
                myItem.unbindStyle()
                if(comboBoxSelectedString === "边"){myItem.bindStyle("Edge")}
                if(comboBoxSelectedString === "面"){myItem.bindStyle("Face")}
                if(comboBoxSelectedString === "块"){myItem.bindStyle("Block")}
                if(comboBoxSelectedString === "组"){myItem.bindStyle("Group")}
            }

            function bindFunction(selectType){
                if(selectType === "边"){
                    myItem.bindStyle("Edge")
                    //console.log("绑定边")
                }
                if(selectType === "面"){
                    myItem.bindStyle("Face")
                    //console.log("绑定面")
                }
                if(selectType === "块"){
                    myItem.bindStyle("Block")
                    //console.log("绑定块")
                }
                if(selectType === "组"){
                    myItem.bindStyle("Group")
                    //console.log("绑定组")
                }
            }
            onComboBoxSelectionChanged:{
                bindFunction(comboBoxSelectedString)
            }

            Component.onCompleted:{
                selector.comboBoxSelectionChanged()
            }
        }
    }
}
