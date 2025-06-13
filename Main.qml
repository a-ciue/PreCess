/**
 * @file Main.qml
 * @brief 程序的交互主界面
 *
 * @sa QObjectList.qml
 * @sa QSelectingBar.qml
 * @sa QSelector.qml
 * @sa QSideBar.qml
 */

import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Controls.Fusion

import fileLoader
import model
import commands

ApplicationWindow {
    id: root
    visibility: Window.Maximized
    title: qsTr("三角剖分交互程序")
    menuBar: MenuBar{
        //height:30
        Menu{
            title: "文件"
            MenuItem{
                text: "指定样条剖分"
                onClicked: splineDialog.open()
            }
            MenuItem{
                text: "导入"
                onClicked: openPatchDialog.open()
            }
            Menu{
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
        Menu{
            title: "编辑"
        }
        Menu{
            title: "视图"
        }
        CommandMenu {
            id: commandMenu
            commands: commandCatalog.qmlCommands()
            sideBar: sideBar
            commandDispatcher: root.commandDispatcher
        }
    }

    required property QModelObserver modelObserver
    required property ModelManager modelManager
    required property QModelQuery modelQuery
    required property QCommandCatalog commandCatalog
    required property CommandDispatcher commandDispatcher

    property bool edgeRenderCheck: false

    ToolBar {
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
                    myItem.resetCamera()
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
        }
    }

    Item{
        id:index
        property bool trigger: false
    }

    StackLayout{
        id:stacklayout
        //anchors.top: root.bottom
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
                        console.log("选中的模型名为：",modelQuery.getModelName(ids.getModelId()))
                        if (ids.size() !== 0 /*&& ids.type() === Element.Edge*/) {
                            commandDispatcher.runCommand(commandCatalog.pathCommand("faceMode.splitEdge"), ids.getModelId(), [ids])
                        }else{
                            console.log("未选中对象或选中对象不是边")
                        }

                        selector.clearSelection()
                    }
                    if(index === 1){
                        // myItem.bindStyle("Face")
                        let ids = myItem.selectedIDs;
                        console.log("选中的模型名为：", modelQuery.getModelName(ids.getModelId()))
                        if (ids.size() !== 0 /*&& ids.type() === Element.Face*/) {
                            commandDispatcher.runCommand(commandCatalog.pathCommand("faceMode.splitFace"), ids.getModelId(), [ids])
                        }else{
                            console.log("未选中对象或选中对象不是面")
                        }

                        selector.clearSelection()
                    }
                }else{
                    if(index === 0){
                        let ids = myItem.selectedIDs;
                        if (ids.size() !== 0) {
                            modelManager.model(ids.getName()).split_edge(ids.ids(0), ids.ids(1), ids.ids(2))
                        }
                    }
                    if(index === 1){
                        let ids = myItem.selectedIDs;
                        if (ids.size() !== 0) {
                            modelManager.model(ids.getName()).split_face(ids.ids(0), ids.ids(1))
                        }
                    }
                }
            }
            onButtonGroupFunction:{
                myItem.unbindStyle()
            }
            onChangeEdgeRender:{
                root.edgeRenderCheck = !root.edgeRenderCheck
                myItem.setEdgeRender(root.edgeRenderCheck)

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
                        let ids = myItem.selectedIDs
                        commandDispatcher.runCommand(commandCatalog.pathCommand("blockMode.mergeBlocks"), ids.getModelId(), [ids])
                        myItem.resetCamera()
                        selector.clearSelection()
                    }
                    else {
                        // myItem.bindStyle("Block")
                        modelManager.model(myItem.selectedIDs.getName()).remesh_block(myItem.selectedIDs)
                        myItem.resetCamera()
                        selector.clearSelection()
                    }
                }else{
                    if(index === 0)
                    {
                        modelManager.model(myItem.selectedIDs.getName()).merge_blocks(myItem.selectedIDs)
                        myItem.resetCamera()
                    }
                    else {
                        modelManager.model(myItem.selectedIDs.getName()).remesh_block(myItem.selectedIDs)
                        myItem.resetCamera()
                    }
                }
            }
            onButtonGroupFunction:{
                myItem.unbindStyle()
            }
            onChangeEdgeRender:{
                root.edgeRenderCheck = !root.edgeRenderCheck
                myItem.setEdgeRender(root.edgeRenderCheck)
            }
        }

    }

    // Rectangle{
    //     id:devider
    //     height:1
    //     color: "black"
    //     anchors.top: stacklayout.bottom
    //     anchors.left: parent.left
    //     anchors.right: myItemRectangle.left
    // }

    ObjectList{
        id:objectList
        anchors.top: stacklayout.bottom
        anchors.left: parent.left
        anchors.right: myItemRectangle.left
        //anchors.bottom: sideBar.top
        width: 160
        height: 200
        objectModel:ListModel{
            id: objectInitializeModel
        }
        Component.onCompleted: {
            modelObserver.modelAdded.connect((model_id)=>objectList.addItem(model_id,modelQuery.getModelName(model_id)))
            modelObserver.modelAdded.connect(myItem.onModelChanged)
            modelObserver.modelChanged.connect(myItem.onModelChanged)

            modelObserver.modelRemoved.connect((modelName)=>{objectList.removeItem(modelName)})
            modelObserver.modelRemoved.connect(myItem.deleteModel)
            objectList.renameModel.connect((oldName,newName)=>{modelManager.renameModel(oldName,newName)})
            //objectList.renameModel.connect(()=>{myItem.setSelectMode("Edge")})
            objectList.removeModel.connect((modelName)=>{modelManager.removeModel(modelName)})
            objectList.changeModelVisibility.connect(myItem.setVisibility)
            objectList.selectionChanged.connect((selectedModel_id)=>{myItem.setSelectModel(selectedModel_id)})
        }
    }

    SideBar{
        id: sideBar
        commandDispatcher: root.commandDispatcher
        curModel: objectList.selectedModel_id
        curSelection: selector.selection
        anchors.top: objectList.bottom
        anchors.left: parent.left
        anchors.right: myItemRectangle.left
        anchors.bottom: parent.bottom
        width: 160
        m:ListModel{
            // ListElement{type: 2; name: "属性甲"; content: "55"}
            // ListElement{type: 2; name: "属性乙"; content: "43"}
            // ListElement{type: 1; name: "属性丙"; content: 1}
            // ListElement{type: 0; name: "属性丁"; content: "无"}
            //ListElement{type: 3; name: "选择器"; content: "无"}//可能会有多个选择器的需求，因此需要动态构造多个选择器
        }
        onSelectModeChanged:{         //应该加上参数以判断是哪一个选择器选择的对象
            //selector.changePropertyEnabled()
            //selector.comboBoxSelectionChanged()
            //console.log("信号被接收")
            selector.changeEnable()
        }
        onCancleCommand:{
            m.clear()
        }

        function changeListElement(){
            if(paraList.count){
                m.remove(0)
            }
            m.append({})
        }
    }

    Page {
        id: myItemRectangle
        anchors.bottom:parent.bottom
        anchors.top:stacklayout.bottom
        anchors.left:objectList.right
        anchors.right:parent.right
        // border {
        //     id: border
        //     width: 5
        // }
        //radius: 5
        Rectangle {
            id: borderRectangle
            anchors.left: myItemRectangle.left
            anchors.right: myItemRectangle.right
            anchors.top: myItemRectangle.top
            height: myItemRectangle.height - 25
            border.color: "black"
            border.width: 3
            //radius: 5
            color: "transparent"
        }

        Page{
            id: renderWindowPage
            anchors.fill: parent
            background: null  // 移除Page的默认背景
            footer: ToolBar{
                height: 25
                // background: Rectangle {  // 为ToolBar添加背景
                //     color: "transparent"
                // }
                RowLayout{
                    anchors.fill: parent
                    ToolButton{
                        text: "边渲染"
                        checkable: true
                        Layout.preferredWidth: 50
                        Layout.fillHeight: true
                        onClicked:{
                            root.edgeRenderCheck = !root.edgeRenderCheck
                            myItem.setEdgeRender(root.edgeRenderCheck)
                        }
                    }
                    ToolButton{
                        text: "块渲染"
                        checkable: true
                        Layout.preferredWidth: 50
                        Layout.fillHeight: true
                        onClicked:{
                            if (checked) {
                                myItem.setRenderMode("Block")
                                stacklayout.currentIndex = 1
                            } else {
                                myItem.setRenderMode("Face")
                                stacklayout.currentIndex = 0
                            }
                        }
                    }
                    Label{
                        Layout.fillWidth: true
                    }
                }
            }

            QRenderWindow {
                id: myItem
                anchors.fill: parent
                anchors.margins: 3  // 调整边距，与border.width对应
                query: modelQuery
            }
        }

        Selector{
            id:selector
            anchors.top:  renderWindowPage.top
            anchors.left: renderWindowPage.left
            anchors.topMargin: 10
            anchors.leftMargin: 10
            property QSelection selection
            enabled: objectList.selectedModelName !== ""  // 绑定到objectList的选中状态

            onClearButtonClicked:{
                clearSelection()
            }

            onConfirmButtonClicked: {
                sideBar.curSelection = myItem.selectedIDs
                sideBar.paraList.currentItem.item.addSelection()
                /*sideBar.savedSelection.push(myItem.selectedIDs)
                // 遍历所有加载的选择器组件
                for (var i in sideBar.parameterList.loadedItems) {
                    var item = sideBar.parameterList.loadedItems[i]
                    console.log("检查组件:", i, item)
                    // 检查是否是选择器组件（type === 3）
                    if (item && item.type === 3) {
                        console.log("找到选择器组件，索引:", i)
                        // 调用 SideBar 的更新函数
                        sideBar.updateSelectorCount(i, comboBoxSelectedString)
                    }
                }*/
            }

            function clearSelection(){
                myItem.clearSelection()
            }

            function bindFunction(selectType){
                if(selectType === "..."){
                    myItem.setSelectMode("None")
                }
                if(selectType === "边"){
                    myItem.setSelectMode("Edge")
                }
                if(selectType === "面"){
                    myItem.setSelectMode("Face")
                }
                if(selectType === "块"){
                    myItem.setSelectMode("Block")
                }
            }

            function changeEnable(){}

            onComboBoxSelectionChanged:{
                bindFunction(comboBoxSelectedString)
            }

            Component.onCompleted:{
                selector.comboBoxSelectionChanged()
            }
        }
    }
}
