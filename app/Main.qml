/**
 * @file Main.qml
 * @brief 程序的交互主界面，使用 KDDockWidgets 可停靠窗口架构
 *
 * @sa ObjectTree.qml
 * @sa Selector.qml
 * @sa SideBar.qml
 * @sa CentralRenderArea.qml
 * @sa JavaScriptConsole.qml
 */

import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Controls.Fusion

import com.kdab.dockwidgets as KDDW

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

    // 基础几何创建暂时直接复用参数侧栏。
    readonly property var createPointInfo: ({
        name: "create_point",
        display_name: qsTr("创建点"),
        description: qsTr("根据三维坐标创建独立几何点"),
        arg_types: [
            { type: QArgType.Float, name: qsTr("X 坐标"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("Y 坐标"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("Z 坐标"), content: "0", description: "" }
        ]
    })

    readonly property var createBoxInfo: ({
        name: "create_box",
        display_name: qsTr("创建长方体"),
        description: qsTr("根据原点和三个轴向尺寸创建长方体"),
        arg_types: [
            { type: QArgType.Float, name: qsTr("原点 X"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("原点 Y"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("原点 Z"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("X 方向长度"), content: "10", description: qsTr("必须大于 0") },
            { type: QArgType.Float, name: qsTr("Y 方向长度"), content: "10", description: qsTr("必须大于 0") },
            { type: QArgType.Float, name: qsTr("Z 方向长度"), content: "10", description: qsTr("必须大于 0") }
        ]
    })

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
                text: qsTr("偏好设置")
                onClicked: preferencesDock.show()
            }
        }
        Menu {
            id: editMenu
            title: qsTr("编辑")
            Repeater {
                model: QModelManager.editSystem.editsInfo
                MenuItem {
                    text: modelData.display_name
                    onTriggered: {
                        var info = modelData
                        App.activeOperation = {
                            info: info,
                            execute: function(model, args) { QModelManager.editSystem.call(info.name, model, args) }
                        }
                    }
                }
            }
        }
        Menu {
            title: qsTr("几何")
            Menu {
                title: qsTr("创建")
                MenuItem {
                    text: qsTr("点")
                    onTriggered: {
                        App.activeOperation = {
                            info: root.createPointInfo,
                            allowWithoutModel: true,
                            showGeometryTarget: true,
                            defaultParameters: [0, 0, 0],
                            execute: function(modelId, args) {
                                var componentId = QModelManager.createPoint(
                                    modelId, App.selection.activeComponentId,
                                    args[0], args[1], args[2])
                                if (componentId >= 0) {
                                    App.selection.activeComponentId = componentId
                                    App.selection.activeModelId = QModelManager.query.findModelIdByComponent(componentId)
                                }
                            }
                        }
                        sideBarDock.show()
                    }
                }
                MenuItem {
                    text: qsTr("长方体")
                    onTriggered: {
                        App.activeOperation = {
                            info: root.createBoxInfo,
                            allowWithoutModel: true,
                            showGeometryTarget: true,
                            defaultParameters: [0, 0, 0, 10, 10, 10],
                            execute: function(modelId, args) {
                                var componentId = QModelManager.createBox(
                                    modelId, App.selection.activeComponentId,
                                    args[0], args[1], args[2],
                                    args[3], args[4], args[5])
                                if (componentId >= 0) {
                                    App.selection.activeComponentId = componentId
                                    App.selection.activeModelId = QModelManager.query.findModelIdByComponent(componentId)
                                }
                            }
                        }
                        sideBarDock.show()
                    }
                }
            }
        }
        Menu{
            title: "视图"
            Action {
                text: "对象树"
                checkable: true
                checked: objectTreeDock.isOpen
                onToggled: {
                    if (objectTreeDock.isOpen) objectTreeDock.close()
                    else objectTreeDock.show()
                }
            }
            Action {
                text: "属性列表"
                checkable: true
                checked: sideBarDock.isOpen
                onToggled: {
                    if (sideBarDock.isOpen) sideBarDock.close()
                    else sideBarDock.show()
                }
            }
            Action {
                text: "控制台"
                checkable: true
                checked: consoleDock.isOpen
                onToggled: {
                    if (consoleDock.isOpen) consoleDock.close()
                    else consoleDock.show()
                }
            }
            Action {
                text: "日志"
                checkable: true
                checked: outputLogDock.isOpen
                onToggled: {
                    if (outputLogDock.isOpen) outputLogDock.close()
                    else outputLogDock.show()
                }
            }
            Action {
                text: "偏好设置"
                checkable: true
                checked: preferencesDock.isOpen
                onToggled: {
                    if (preferencesDock.isOpen) preferencesDock.close()
                    else preferencesDock.show()
                }
            }
        }
        Menu {
            id: commandMenu
            title: qsTr("算法")
            Repeater {
                model: QModelManager.algorithmSystem.algorithmsInfo
                MenuItem {
                    text: modelData.display_name
                    onTriggered: {
                        var info = modelData
                        App.activeOperation = {
                            info: info,
                            execute: function(model, args) { QModelManager.algorithmSystem.call(info.name, model, args) }
                        }
                    }
                }
            }
        }
    }

    MessageDialog {
        id: geometryOperationErrorDialog
        title: qsTr("几何操作失败")
        buttons: MessageDialog.Ok
    }

    Connections {
        target: QModelManager
        function onGeometryOperationFailed(message) {
            geometryOperationErrorDialog.text = message
            geometryOperationErrorDialog.open()
        }
    }

    Shortcut {
        sequence: "F10"
        onActivated: {
            if (consoleDock.isOpen)
                consoleDock.close()
            else
                consoleDock.show()
        }
    }

    KDDW.DockingArea {
        id: dockingArea
        anchors.fill: parent
        options: KDDW.KDDockWidgets.MainWindowOption_HasCentralWidget
        persistentCentralItemFileName: "qrc:/qt/qml/app/CentralRenderArea.qml"
        uniqueName: "PreCessMainLayout"

        KDDW.DockWidget {
            id: objectTreeDock
            uniqueName: "objectTree"
            title: "对象树"
            ObjectTree {
                anchors.fill: parent
            }
        }

        KDDW.DockWidget {
            id: sideBarDock
            uniqueName: "sideBar"
            title: "属性列表"
            SideBar {
                anchors.fill: parent
            }
        }

        KDDW.DockWidget {
            id: consoleDock
            uniqueName: "console"
            title: "控制台"

            JavaScriptConsole {
                anchors.fill: parent
            }
        }

        KDDW.DockWidget {
            id: outputLogDock
            uniqueName: "outputLog"
            title: "日志"

            OutputLog {
                anchors.fill: parent
            }
        }

        KDDW.DockWidget {
            id: preferencesDock
            uniqueName: "preferences"
            title: "偏好设置"

            PreferencesWindow {
                anchors.fill: parent
            }
        }

        Component.onCompleted: {
            addDockWidget(objectTreeDock, KDDW.KDDockWidgets.Location_OnLeft, null, Qt.size(250, 0))
            addDockWidget(sideBarDock, KDDW.KDDockWidgets.Location_OnBottom, objectTreeDock, Qt.size(0, 400))
            addDockWidget(consoleDock, KDDW.KDDockWidgets.Location_OnBottom, null, Qt.size(0, 300), KDDW.KDDockWidgets.StartHidden)
            addDockWidget(outputLogDock, KDDW.KDDockWidgets.Location_OnBottom, null, Qt.size(0, 300), KDDW.KDDockWidgets.StartHidden)
            addDockWidget(preferencesDock, KDDW.KDDockWidgets.Location_OnTop, objectTreeDock, Qt.size(0, 200), KDDW.KDDockWidgets.StartHidden)
        }
    }

    KDDW.LayoutSaver { id: layoutSaver }
    
    //打开文件对话框
    FileDialog {
        id: openPatchDialog
        nameFilters: QModelManager.ioSystem.dialogNameFilters
        onAccepted: {
            if (selectedNameFilter.index >= 0) {
                QModelManager.ioSystem.read(selectedNameFilter.name, selectedFile, [])
                App.registry.renderWindow.resetCamera()
            } else {
                console.exception("No valid file type selected.")
            }
        }
    }

    //保存文件对话框
    FileDialog {
        id: saveFaceDialog
        nameFilters: QModelManager.ioSystem.dialogNameFilters
        fileMode: FileDialog.SaveFile
        onAccepted: {
            if (selectedNameFilter.index >= 0) {
                QModelManager.ioSystem.write(selectedNameFilter.name, App.selection.activeModelId, selectedFile, [])
            } else {
                console.exception("No valid file type selected.")
            }
        }
    }

    Component.onCompleted: {
        Qt.callLater(function() {
            for (let i = 0; i < commandLineArgs.length; ++i) {
                let ok = QModelManager.ioSystem.read("All files", commandLineArgs[i], [])
                if (!ok) {
                    console.exception("启动打开失败: " + commandLineArgs[i]);
                }
            }
        })
    }
}
