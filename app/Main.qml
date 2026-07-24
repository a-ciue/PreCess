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
import app.model.systems.io

import app.render

ApplicationWindow {
    id: root 
    width: 800
    height: 600
    visibility: Window.Maximized
    title: qsTr("PreCess")

    GeometryOperationActions {
        id: geometryActions
        onOperationActivated: sideBarDock.show()
    }

    header: AppToolbar {
        geometryOperationActions: geometryActions
        windowHeight: root.height
        objectTreeOpen: objectTreeDock.isOpen
        propertyListOpen: sideBarDock.isOpen
        attributeRenderOpen: attributeRenderDock.isOpen
        consoleOpen: consoleDock.isOpen
        outputLogOpen: outputLogDock.isOpen
        preferencesOpen: preferencesDock.isOpen
        onImportRequested: openPatchDialog.open()
        onExportRequested: saveFaceDialog.open()
        onObjectTreeToggled: {
            if (objectTreeDock.isOpen) objectTreeDock.close()
            else objectTreeDock.show()
        }
        onPropertyListToggled: {
            if (sideBarDock.isOpen) sideBarDock.close()
            else sideBarDock.show()
        }
        onAttributeRenderToggled: {
            if (attributeRenderDock.isOpen) attributeRenderDock.close()
            else attributeRenderDock.show()
        }
        onConsoleToggled: {
            if (consoleDock.isOpen) consoleDock.close()
            else consoleDock.show()
        }
        onOutputLogToggled: {
            if (outputLogDock.isOpen) outputLogDock.close()
            else outputLogDock.show()
        }
        onPreferencesToggled: {
            if (preferencesDock.isOpen) preferencesDock.close()
            else preferencesDock.show()
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

    Connections {
        target: App.selection
        function onActiveModelIdChanged() {
            QModelManager.featureSystem.setActiveModel(App.selection.activeModelId)
        }
        function onActiveComponentIdChanged() {
            QModelManager.featureSystem.setActiveComponent(App.selection.activeComponentId)
        }
    }

    Connections {
        target: QModelManager
        function onModelAdded(id) {
            if (App.registry.renderWindow)
                App.registry.renderWindow.clearSelection()
        }
        function onModelRemoved(id) {
            if (App.registry.renderWindow)
                App.registry.renderWindow.clearSelection()
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
            addDockWidget(attributeRenderDock, KDDW.KDDockWidgets.Location_OnBottom, objectTreeDock, Qt.size(0, 300), KDDW.KDDockWidgets.StartHidden)
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
        // 同步当前活动模型/组件到功能系统（功能菜单由 AppToolbar 自行构建）
        QModelManager.featureSystem.setActiveModel(App.selection.activeModelId)
        QModelManager.featureSystem.setActiveComponent(App.selection.activeComponentId)

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
