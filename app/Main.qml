/**
 * @file Main.qml
 * @brief 程序的交互主界面
 *
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
import app.model.systems
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
            MenuSeparator{}
            MenuItem{
                text: qsTr("插件管理")
                onClicked: pluginManagerDialog.open()
            }
        }
        Menu {
            id: editMenu
            title: qsTr("编辑")
            Repeater {
                model: editSystem.editsInfo
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
                model: algorithmSystem.algorithmsInfo
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

    // ===== modelObserver → objectTree / myItem =====
    Timer {
        id: refreshTimer
        interval: 100
        repeat: false
        onTriggered: objectTree.refreshTree()
    }

    function resetSelectionState() {
        objectTree.curComponentId = -1
        objectTree.curModelId = -1
        myItem.setSelectMode("None")
        myItem.clearSelection()
    }

    Connections {
        target: modelObserver

        function onModelAdded(modelId)   { refreshTimer.restart(); myItem.onModelChanged(modelId); resetSelectionState() }
        function onModelChanged(modelId) { refreshTimer.restart(); myItem.onModelChanged(modelId); resetSelectionState() }
        function onModelRemoved(modelId) { refreshTimer.restart(); myItem.deleteModel(modelId); resetSelectionState() }
        function onComponentRemoved(componentId) { refreshTimer.restart(); myItem.deleteComponent(componentId); resetSelectionState() }
    }

    StackLayout{
        id:stacklayout
        anchors.left: parent.left
        anchors.right: parent.right
        height: 0
    }

    ObjectTree {
        id: objectTree
        anchors.top: stacklayout.bottom
        anchors.left: parent.left
        width: 250
        height: parent.height * 0.5
        modelQuery: root.modelQuery

        onSelectionChanged: (componentId) => myItem.setSelectComponent(componentId)

        onDeleteRequested: (nodeId, depth) => {
            if (depth === 0) modelManager.removeModel(nodeId)
            else if (depth === 1) modelManager.removeComponent(nodeId)
        }

        onVisibilityChanged: (nodeId, depth, visible) => {
            if (depth === 0) myItem.setVisibility(nodeId, visible)
            else if (depth === 1) myItem.setComponentVisibility(nodeId, visible)
        }
    }

    SideBar{
        id: sideBar
        curComponent: objectTree.curComponentId
        confirm_listener: selector.confirm_listener
        anchors.top: objectTree.bottom
        anchors.left: parent.left
        anchors.right: myItemRectangle.left
        anchors.bottom: parent.bottom
        width: 250
        onSelectModeChanged:{         //应该加上参数以判断是哪一个选择器选择的对象
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
        anchors.left:objectTree.right
        anchors.right:parent.right
        Rectangle {
            id: borderRectangle
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: myItemRectangle.height - 25
            border.color: "black"
            border.width: 3
            color: "transparent"
        }

        Page{
            id: renderWindowPage
            anchors.fill: parent
            background: null  // 移除Page的默认背景
            footer: ToolBar{
                height: 25
                RowLayout{
                    anchors.fill: parent
                    ToolButton{
                        text: "边渲染"
                        checkable: true
                        checked: myItem.cur_edge_render
                        Layout.preferredWidth: 50
                        Layout.fillHeight: true
                        onClicked:{
                            myItem.setEdgeRender(objectTree.curModelId, !myItem.cur_edge_render)
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
                                myItem.setRenderMode(objectTree.curModelId, "Block")
                            } else {
                                myItem.setRenderMode(objectTree.curModelId, "Face")
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
            cur_model: objectTree.curComponentId
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

    //打开文件对话框
    FileDialog {
        id: openPatchDialog
        nameFilters: ioSystem.dialogNameFilters
        onAccepted: {
            if (selectedNameFilter.index >= 0) {
                ioSystem.read(selectedNameFilter.name, selectedFile, [])
                myItem.resetCamera()
            } else {
                console.exception("No valid file type selected.")
            }
        }
    }

    //保存文件对话框
    FileDialog {
        id: saveFaceDialog
        nameFilters: ioSystem.dialogNameFilters
        fileMode: FileDialog.SaveFile
        onAccepted: {
            if (selectedNameFilter.index >= 0) {
                ioSystem.write(selectedNameFilter.name, objectTree.curModelId, selectedFile, [])
            } else {
                console.exception("No valid file type selected.")
            }
        }
    }

    //JavaScript控制台
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

    //插件管理对话框
    Dialog {
        id: pluginManagerDialog
        title: qsTr("插件管理")
        standardButtons: DialogButtonBox.NoButton
        modal: true
        anchors.centerIn: parent
        width: 400
        height: 300

        PluginManagerComponent {
            id: pluginManagerComponent
            anchors.fill: parent
            pluginManager: root.modelManager.systemPluginManager
        }
    }

    Component.onCompleted: {
        Qt.callLater(function() {
            for (let i = 0; i < commandLineArgs.length; ++i) {
                let ok = ioSystem.read("All files", commandLineArgs[i], []);
                if (!ok) {
                    console.exception("启动打开失败: " + commandLineArgs[i]);
                }
            }

            myItem.resetCamera();
        });
    }
}
