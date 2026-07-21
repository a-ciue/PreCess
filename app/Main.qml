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
import app.model.systems.feature
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

    menuBar: MenuBar{
        id: menuBar
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
                    onTriggered: geometryActions.startCreatePoint()
                }
                Menu {
                    title: qsTr("直线边")
                    MenuItem {
                        text: qsTr("输入两点坐标")
                        onTriggered: geometryActions.startCreateLineByCoordinates()
                    }
                    MenuItem {
                        text: qsTr("选择两个已有点")
                        onTriggered: geometryActions.startCreateLineFromVertices()
                    }
                }
                Menu {
                    title: qsTr("面")
                    MenuItem {
                        text: qsTr("矩形面")
                        onTriggered: geometryActions.startCreateRectangleFace()
                    }
                    MenuItem {
                        text: qsTr("圆盘/扇形面")
                        onTriggered: geometryActions.startCreateDiskFace()
                    }
                    MenuItem {
                        text: qsTr("选择闭合边创建面")
                        onTriggered: geometryActions.startCreateFaceFromEdges()
                    }
                }
                Menu {
                    title: qsTr("体")
                    MenuItem {
                        text: qsTr("长方体")
                        onTriggered: geometryActions.startCreateBox()
                    }
                    MenuItem {
                        text: qsTr("圆柱体")
                        onTriggered: geometryActions.startCreateCylinder()
                    }
                    MenuItem {
                        text: qsTr("圆锥/圆台")
                        onTriggered: geometryActions.startCreateCone()
                    }
                    MenuItem {
                        text: qsTr("球体/部分球体")
                        onTriggered: geometryActions.startCreateSphere()
                    }
                    MenuItem {
                        text: qsTr("拉伸面为实体")
                        onTriggered: geometryActions.startExtrudeFace()
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
                            execute: function(component, args) {
                                QModelManager.algorithmSystem.call(info.name, component, args)
                            }
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

    // 功能菜单：按功能声明的 menu_path 分组动态生成（功能注册/注销时重建）
    property var _featureMenus: []

    function rebuildFeatureMenus() {
        // 移除旧的功能菜单
        for (let old_menu of root._featureMenus) {
            menuBar.removeMenu(old_menu)
            old_menu.destroy()
        }
        root._featureMenus = []

        // 按 menu_path 分组
        let groups = {}
        for (let info of QModelManager.featureSystem.featuresInfo) {
            let path = info.menu_path || "功能"
            if (!groups[path])
                groups[path] = []
            groups[path].push(info)
        }
        for (let path in groups) {
            let menu = featureMenuComponent.createObject(menuBar, { "title": path })
            for (let info of groups[path]) {
                let item = featureMenuItemComponent.createObject(menu, { "text": info.display_name })
                item.triggered.connect(function() {
                    App.activeOperation = {
                        info: info,
                        isFeature: true,
                        execute: function() { return QModelManager.featureSystem.invoke(info.name) }
                    }
                })
                menu.addItem(item)
            }
            menuBar.addMenu(menu)
            root._featureMenus.push(menu)
        }
    }

    Component {
        id: featureMenuComponent
        Menu {
        }
    }
    Component {
        id: featureMenuItemComponent
        MenuItem {
        }
    }

    Connections {
        target: QModelManager.featureSystem
        function onFeaturesInfoChanged() {
            root.rebuildFeatureMenus()
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
        // 构建功能菜单并同步当前活动模型/组件到功能系统
        rebuildFeatureMenus()
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
