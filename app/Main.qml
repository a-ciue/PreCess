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
                text: "属性渲染"
                checkable: true
                checked: attributeRenderDock.isOpen
                onToggled: {
                    if (attributeRenderDock.isOpen) attributeRenderDock.close()
                    else attributeRenderDock.show()
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

    Shortcut {
        sequence: "F10"
        onActivated: {
            if (consoleDock.isOpen)
                consoleDock.close()
            else
                consoleDock.show()
        }
    }

    StackLayout{
        id:stacklayout
        anchors.left: parent.left
        anchors.right: parent.right
        height: 0
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
            id: attributeRenderDock
            uniqueName: "attributeRender"
            title: "属性渲染"
            AttributeRenderPanel {
                anchors.fill: parent
            }
        }

        KDDW.DockWidget {
            id: consoleDock
            uniqueName: "console"
            title: "控制台"

            JavaScriptConsole {
                anchors.fill: parent
                onCloseRequested: consoleDock.close()
            }
        }

        Component.onCompleted: {
            addDockWidget(objectTreeDock, KDDW.KDDockWidgets.Location_OnLeft, null, Qt.size(250, 0))
            addDockWidget(sideBarDock, KDDW.KDDockWidgets.Location_OnBottom, objectTreeDock, Qt.size(0, 400))
            addDockWidget(attributeRenderDock, KDDW.KDDockWidgets.Location_OnBottom, objectTreeDock, Qt.size(0, 500), KDDW.KDDockWidgets.StartHidden)
            addDockWidget(consoleDock, KDDW.KDDockWidgets.Location_OnBottom, null, Qt.size(0, 300), KDDW.KDDockWidgets.StartHidden)
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
