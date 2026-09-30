/**
 * @file Main.qml
 * @brief 程序的交互主界面，使用内嵌停靠组件架构
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
import QtQuick.Controls.Fusion

import QtCore

import PreCess.Docking as Docking

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
    flags: Qt.platform.os === "wasm" ? (Qt.Window | Qt.FramelessWindowHint) : Qt.Window

    // 布局持久化：JSON 快照存于 QSettings；退出保存，启动恢复
    Settings {
        id: dockSettings
        category: "DockLayout"
        property string json
    }

    //! @brief 声明默认停靠布局（首次启动与恢复前的基础布局）
    function applyDefaultLayout() {
        dockHost.placePanel(objectTreeDock, Docking.Tokens.DockEdge.Left, null, Qt.size(250, 0))
        dockHost.placePanel(sideBarDock, Docking.Tokens.DockEdge.Bottom, objectTreeDock, Qt.size(0, 400))
        dockHost.placePanel(attributeRenderDock, Docking.Tokens.DockEdge.Bottom, objectTreeDock, Qt.size(0, 300), Docking.Tokens.PanelLaunch.Hidden)
        dockHost.placePanel(consoleDock, Docking.Tokens.DockEdge.Bottom, null, Qt.size(0, 300), Docking.Tokens.PanelLaunch.Hidden)
        dockHost.placePanel(pythonConsoleDock, Docking.Tokens.DockEdge.Right, null, Qt.size(450, 0), Docking.Tokens.PanelLaunch.Hidden)
        dockHost.placePanel(outputLogDock, Docking.Tokens.DockEdge.Bottom, null, Qt.size(0, 300), Docking.Tokens.PanelLaunch.Hidden)
        dockHost.placePanel(preferencesDock, Docking.Tokens.DockEdge.Top, objectTreeDock, Qt.size(0, 200), Docking.Tokens.PanelLaunch.Hidden)
    }

    onClosing: dockSettings.json = dockHost.saveLayout()

    header: AppToolbar {
        windowHeight: root.height
        objectTreeOpen: objectTreeDock.shown
        propertyListOpen: sideBarDock.shown
        attributeRenderOpen: attributeRenderDock.shown
        consoleOpen: consoleDock.shown
        pythonConsoleOpen: pythonConsoleDock.shown
        outputLogOpen: outputLogDock.shown
        preferencesOpen: preferencesDock.shown
        onObjectTreeToggled: {
            if (objectTreeDock.shown) objectTreeDock.hidePanel()
            else objectTreeDock.showPanel()
        }
        onPropertyListToggled: {
            if (sideBarDock.shown) sideBarDock.hidePanel()
            else sideBarDock.showPanel()
        }
        onAttributeRenderToggled: {
            if (attributeRenderDock.shown) attributeRenderDock.hidePanel()
            else attributeRenderDock.showPanel()
        }
        onConsoleToggled: {
            if (consoleDock.shown) consoleDock.hidePanel()
            else consoleDock.showPanel()
        }
        onPythonConsoleToggled: {
            if (pythonConsoleDock.shown) pythonConsoleDock.hidePanel()
            else pythonConsoleDock.showPanel()
        }
        onOutputLogToggled: {
            if (outputLogDock.shown) outputLogDock.hidePanel()
            else outputLogDock.showPanel()
        }
        onPreferencesToggled: {
            if (preferencesDock.shown) preferencesDock.hidePanel()
            else preferencesDock.showPanel()
        }
    }

    Shortcut {
        sequence: "F10"
        onActivated: {
            if (consoleDock.shown)
                consoleDock.hidePanel()
            else
                consoleDock.showPanel()
        }
    }

    Shortcut {
        sequence: "F11"
        onActivated: {
            if (pythonConsoleDock.shown)
                pythonConsoleDock.hidePanel()
            else
                pythonConsoleDock.showPanel()
        }
    }

    // 撤销/重做快捷键（栈空时适配器内部空转）
    Shortcut {
        sequence: "Ctrl+Z"
        onActivated: QModelManager.undoStack.undo()
    }
    Shortcut {
        sequence: "Ctrl+Y"
        onActivated: QModelManager.undoStack.redo()
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

    Docking.DockHost {
        id: dockHost
        anchors.fill: parent

        centralItemFile: "qrc:/qt/qml/app/CentralRenderArea.qml"
        uniqueName: "PreCessMainLayout"

        Docking.DockPanel {
            id: objectTreeDock
            uniqueName: "objectTree"
            title: "对象树"
            ObjectTree {
                anchors.fill: parent
            }
        }

        Docking.DockPanel {
            id: sideBarDock
            uniqueName: "sideBar"
            title: "操作面板"
            SideBar {
                anchors.fill: parent
            }
        }

        Docking.DockPanel {
            id: attributeRenderDock
            uniqueName: "attributeRender"
            title: "属性渲染"
            AttributeRenderPanel {
                anchors.fill: parent
            }
        }

        Docking.DockPanel {
            id: consoleDock
            uniqueName: "console"
            title: "JavaScript 控制台"

            JavaScriptConsole {
                anchors.fill: parent
            }
        }

        Docking.DockPanel {
            id: pythonConsoleDock
            uniqueName: "pythonConsole"
            title: "Python 控制台"

            PythonConsole {
                anchors.fill: parent
            }
        }

        Docking.DockPanel {
            id: outputLogDock
            uniqueName: "outputLog"
            title: "日志"

            OutputLog {
                anchors.fill: parent
            }
        }

        Docking.DockPanel {
            id: preferencesDock
            uniqueName: "preferences"
            title: "偏好设置"

            PreferencesWindow {
                anchors.fill: parent
            }
        }

        Component.onCompleted: {
            // 先应用声明默认布局，再尝试恢复上次保存的快照
            root.applyDefaultLayout()
            if (dockSettings.json.length > 0)
                dockHost.restoreLayout(dockSettings.json)
        }
    }


    // 拖拽导入：一次可拖入多个文件，逐个交给 read；能否导入由 C++ 判定并写日志
    DropArea {
        id: importDropArea
        anchors.fill: parent
        z: 1

        // 不做前置过滤：是否可导入由 C++ read 判定（失败会记 error 日志）
        onEntered: {
            drag.accepted = drag.urls.length > 0
        }
        onDropped: {
            drop.accepted = drop.urls.length > 0
            if (drop.urls.length === 0)
                return

            // 逐个导入并记录失败项，全部导入结束后统一重置视角
            let failed = 0
            for (const url of drop.urls) {
                if (!QModelManager.ioSystem.read("All files", url, []))
                    ++failed
            }
            if (failed < drop.urls.length && App.registry.renderWindow)
                App.registry.renderWindow.resetCamera()
            if (failed > 0)
                outputLogDock.showPanel() // 失败原因由日志面板承载，直接打开便于查看
        }

        // 拖入可导入文件时的高亮提示
        Rectangle {
            anchors.fill: parent
            visible: importDropArea.containsDrag
            color: Qt.rgba(0.29, 0.56, 0.89, 0.12)
            border.color: "#4a90e2"
            border.width: 2

            Label {
                anchors.centerIn: parent
                text: qsTr("松开鼠标以导入模型文件")
                font.pixelSize: 16
                color: "#1a6fc4"
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
