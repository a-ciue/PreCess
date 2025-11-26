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

import app.model
import app.core
import app.model.systems.algo
import app.model.systems.io
import app.model.systems.edit
import app.render

ApplicationWindow {
    id: root 
    width: 800
    height: 600
    visibility: Window.Maximized
    title: qsTr("PreCess")

    // 控制台可视化全局参数
    property bool consoleVisible: false

    // 使用Shortcut组件代替Keys处理，更可靠
    Shortcut {
        sequence: "F10"
        onActivated: {
            root.consoleVisible = !root.consoleVisible
        }
    }

    menuBar: MenuBar{
        //height:30
        Menu{
            title: "文件"
            MenuItem{
                text: "导入..."
                onClicked: openPatchDialog.open()
            }
            MenuItem{
                text: "导出..."
                onClicked: saveFaceDialog.open()
            }
        }
        Menu {
            id: editMenu
            title: qsTr("编辑")
            Repeater {
                model: editSystem.getEditsInfo()
                MenuItem {
                    text: modelData.display_name
                    onTriggered: {
                        sideBar.curAlgoInfo = modelData
                        sideBar.system = editSystem
                    }
                }
            }
        }
        Menu{
            title: "视图"
        }
        Menu {
            id: commandMenu
            title: qsTr("算法")
            Repeater {
                model: algorithmSystem.getAlgorithmsInfo()
                MenuItem {
                    text: modelData.display_name
                    onTriggered: {
                        sideBar.curAlgoInfo = modelData
                        sideBar.system = algorithmSystem
                    }
                }
            }
        }
    }

    required property QModelObserver modelObserver
    required property QModelManager modelManager
    required property QModelQuery modelQuery
    required property QAlgorithmSystemAdaptor algorithmSystem
    required property QModelIOSystemAdaptor ioSystem
    required property QEditSystemAdaptor editSystem

    ToolBar {
        id: header
        RowLayout {
            anchors.fill: parent

            FileDialog {
                id: openPatchDialog
                nameFilters: ioSystem.getDialogNameFilters()
                onAccepted: {
                    if (selectedNameFilter.index >= 0) {
                        ioSystem.read(selectedNameFilter.name, selectedFile, [])
                        myItem.resetCamera()
                    } else {
                        console.exception("No valid file type selected.")
                    }
                }
            }
            FileDialog {
                id: saveFaceDialog
                nameFilters: ioSystem.getDialogNameFilters()
                fileMode: FileDialog.SaveFile
                onAccepted: {
                    if (selectedNameFilter.index >= 0) {
                        ioSystem.write(selectedNameFilter.name, objectList.curModelId, selectedFile, [])
                    } else {
                        console.exception("No valid file type selected.")
                    }
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
                            algorithmSystem.call("faceMode.splitEdge", ids.getModelId(), [ids])
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
                            algorithmSystem.call("faceMode.splitFace", ids.getModelId(), [ids])
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
                myItem.setEdgeRender(objectList.curModelId, !myItem.cur_edge_render)
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
                        algorithmSystem.call("blockMode.mergeBlocks", ids.getModelId(), [ids])
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
                myItem.setEdgeRender(objectList.curModelId, !myItem.cur_edge_render)
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
        width: 250
        height: 200
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
        curModel: objectList.curModelId
        confirm_listener: selector.confirm_listener
        anchors.top: objectList.bottom
        anchors.left: parent.left
        anchors.right: myItemRectangle.left
        anchors.bottom: parent.bottom
        width: 250
        onSelectModeChanged:{         //应该加上参数以判断是哪一个选择器选择的对象
            //selector.changePropertyEnabled()
            //selector.comboBoxSelectionChanged()
            //console.log("信号被接收")
            selector.changeEnable()
        }
        onCancleCommand:{
            m.clear()
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
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
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
                        checked: myItem.cur_edge_render
                        Layout.preferredWidth: 50
                        Layout.fillHeight: true
                        onClicked:{
                            myItem.setEdgeRender(objectList.curModelId, !myItem.cur_edge_render)
                        }
                    }
                    ToolButton{
                        text: "点渲染"
                        checkable: true
                        checked: myItem.cur_vertex_render
                        Layout.preferredWidth: 50
                        Layout.fillHeight: true
                        onClicked:{
                            myItem.cur_vertex_render = !myItem.cur_vertex_render
                        }
                    }
                    ToolButton{
                        text: "块渲染"
                        checkable: true
                        Layout.preferredWidth: 50
                        Layout.fillHeight: true
                        onClicked:{
                            if (checked) {
                                myItem.setRenderMode(objectList.curModelId, "Block")
                                stacklayout.currentIndex = 1
                            } else {
                                myItem.setRenderMode(objectList.curModelId, "Face")
                                stacklayout.currentIndex = 0
                            }
                        }
                    }
                    ToolButton{
                        text: "裁剪"
                        checkable: true
                        Layout.preferredWidth: 50
                        Layout.fillHeight: true
                        onClicked:{
                            if (checked) {
                                myItem.setMeshClip(true)
                            } else {
                                myItem.setMeshClip(false)
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
            cur_model: objectList.curModelId
            anchors.top:  renderWindowPage.top
            anchors.left: renderWindowPage.left
            anchors.topMargin: 10
            anchors.leftMargin: 10

            onClearButtonClicked:{
                clearSelection()
            }

            onConfirmButtonClicked: {
                selector.selection = myItem.selectedIDs
            }

            function clearSelection(){
                myItem.clearSelection()
            }

            function bindFunction(selectType){
                if(selectType === "..."){
                    myItem.setSelectMode("None")
                }
                if(selectType === "点"){
                    myItem.setSelectMode("Vertex")
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
                if(selectType === "体"){
                    myItem.setSelectMode("Solid")
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

    JavaScriptConsole {
        id: myConsole
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: parent.height * 0.3
        z: 1000  // 确保在最上层显示
        consoleVisible: root.consoleVisible
        
        onCloseRequested: {
            root.consoleVisible = false
        }
    }
}
