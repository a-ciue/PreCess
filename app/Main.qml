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
import QtQuick.Controls
import QtQuick.Layouts

import QtCore

import PreCess.Docking as Docking

import app.model
import app.core
import app.model.systems
import app.model.systems.io
import app.model.systems.algo

import app.render

ApplicationWindow {
    id: root
    width: 800
    height: 600
    visibility: Window.Maximized
    title: qsTr("ZenithGrid")
    color: Theme.windowBackground
    flags: Qt.platform.os === "wasm" ? (Qt.Window | Qt.FramelessWindowHint) : Qt.Window

    // 壁纸背景层：纯色兜底 + 双缓冲图片淡入淡出（无壁纸时露出 windowBackground）
    background: Item {
        Rectangle {
            anchors.fill: parent
            color: Theme.windowBackground
        }

        // 当前图：承接上一次过渡结果
        Image {
            id: wallpaperCurrent
            anchors.fill: parent
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            visible: source != ""
        }

        // 新图：加载完成后淡入，结束后交给 current（避免切换闪烁）
        Image {
            id: wallpaperIncoming
            anchors.fill: parent
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            visible: source != ""
            opacity: 0

            onStatusChanged: {
                if (status === Image.Ready)
                    wallpaperFadeIn.restart()
            }
        }

        NumberAnimation {
            id: wallpaperFadeIn
            target: wallpaperIncoming
            property: "opacity"
            from: 0
            to: 1
            duration: 400
            easing.type: Easing.InOutCubic
            onFinished: {
                wallpaperCurrent.source = wallpaperIncoming.source
                wallpaperIncoming.source = ""
                wallpaperIncoming.opacity = 0
            }
        }

        Connections {
            target: Theme
            function onWallpaperChanged() {
                // 以字符串壁纸 id 为准：url 与字符串用 === 比较不可靠，
                // 会导致清空分支被跳过、壁纸残留
                const id = Theme.wallpaper
                if (id.length === 0) {
                    wallpaperCurrent.source = ""
                    wallpaperIncoming.source = ""
                    wallpaperIncoming.opacity = 0
                    return
                }
                wallpaperIncoming.opacity = 0
                wallpaperIncoming.source = "image://wallpaper/" + id
            }
        }

        Component.onCompleted: wallpaperCurrent.source = Theme.wallpaperSource
    }

    // 冻结任务忙碌态（聚合位经共享任务状态源单点暴露）：算法执行 ∨ 功能冻结任务——
    // 忙碌遮罩、快捷键与工具栏禁用据此驱动；自由任务不置位（不打断用户操作）
    readonly property bool frozenBusy: QModelManager.taskStatus.frozenBusy
    // 操作状态由窗口持有，侧栏隐藏或重建不会结束当前操作。
    readonly property OperationSession operationSession: OperationSession {}

    // 调色板统一由 AppPalette 写入应用调色板（随 Theme.dark 同步），窗口/控件
    // 与弹窗均据此渲染，避免在此重复列举角色色值。

    // 把应用主题明暗推送给 dock 模块（dock 刻意不反向依赖 app）
    Binding {
        target: Docking.Theme
        property: "dark"
        value: Theme.dark
    }

    // 把自定义壁纸图像源推送给 dock 模块（浮动窗口经 Docking.Theme.wallpaperUrl 读取）
    Binding {
        target: Docking.Theme
        property: "wallpaperUrl"
        value: Theme.wallpaperSource
    }

    // 同步应用调色板：ToolTip 等按应用调色板取色的弹窗据此跟随主题
    Binding {
        target: AppPalette
        property: "dark"
        value: Theme.dark
    }

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

    // 保留本次启动的默认布局，供恢复被关闭、拖出屏幕或压缩的面板。
    property string defaultDockLayout: ""

    onClosing: dockSettings.json = dockHost.saveLayout()

    header: AppToolbar {
        windowHeight: root.height
        leadingFeatureMenus: ["网格生成算法"]
        onResetLayoutRequested: {
            if (root.defaultDockLayout.length > 0)
                dockHost.restoreLayout(root.defaultDockLayout)
        }
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

    // 底部状态栏：进度/取消红叉/百分比/回写待定徽标——控件独立（TaskStatusBar.qml），
    // 数据源统一 QModelManager.taskStatus（共享任务状态源，不点名具体系统）
    footer: TaskStatusBar {
        id: statusBar
    }

    Shortcut {
        sequence: "F10"
        enabled: !root.frozenBusy
        onActivated: {
            if (consoleDock.shown)
                consoleDock.hidePanel()
            else
                consoleDock.showPanel()
        }
    }

    Shortcut {
        sequence: "F11"
        enabled: !root.frozenBusy
        onActivated: {
            if (pythonConsoleDock.shown)
                pythonConsoleDock.hidePanel()
            else
                pythonConsoleDock.showPanel()
        }
    }

    // 撤销/重做快捷键（栈空时适配器内部空转；算法执行期间禁用防重入）
    Shortcut {
        sequence: "Ctrl+Z"
        enabled: !root.frozenBusy
        onActivated: QModelManager.undoStack.undo()
    }
    Shortcut {
        sequence: "Ctrl+Y"
        enabled: !root.frozenBusy
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
                // 停靠层以内容的隐式尺寸作为最小尺寸，防止对象树被挤到零高度。
                implicitWidth: 200
                implicitHeight: 180
                anchors.fill: parent
            }
        }

        Docking.DockPanel {
            id: sideBarDock
            uniqueName: "sideBar"
            title: sideBarContent.panelTitle
            SideBar {
                id: sideBarContent
                session: root.operationSession
                implicitWidth: 200
                implicitHeight: 120
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
            // 等窗口完成首轮布局后再按实际尺寸分配面板占比。
            Qt.callLater(function() {
                root.applyDefaultLayout()
                root.defaultDockLayout = dockHost.saveLayout()
                if (dockSettings.json.length > 0)
                    dockHost.restoreLayout(dockSettings.json)
            })
        }
    }


    // 拖拽导入：一次可拖入多个文件，逐个交给 read；能否导入由 C++ 判定并写日志
    DropArea {
        id: importDropArea
        anchors.fill: parent
        z: 1
        onContainsDragChanged: App.importDragActive = containsDrag

        // 不做前置过滤：是否可导入由 C++ read 判定（失败会记 error 日志）
        onEntered: {
            drag.accepted = drag.urls.length > 0
        }
        onDropped: {
            drop.accepted = drop.urls.length > 0
            if (drop.urls.length === 0)
                return
            // 算法执行期间拒绝导入：模型须保持冻结
            if (root.frozenBusy) {
                drop.accepted = false
                return
            }

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
    }

    // 算法执行期间的忙碌遮罩：吞掉鼠标交互并给出"阻塞中"反馈（footer 状态栏在遮罩之外）
    Rectangle {
        id: busyMask
        z: 100
        visible: root.frozenBusy
        anchors.top: root.header.bottom
        anchors.bottom: root.footer.top
        anchors.left: parent.left
        anchors.right: parent.right
        color: Theme.busyMaskOverlay

        MouseArea {
            anchors.fill: parent
            // 默认只接收左键，右键、中键和滚轮须在遮罩内明确拦截。
            acceptedButtons: Qt.AllButtons
            onWheel: (wheel) => { wheel.accepted = true; }
        }

        // hover 独立于鼠标按键分发，显式阻止下层渲染控件与 HoverHandler。
        HoverHandler {
            blocking: true
        }

        Label {
            anchors.centerIn: parent
            font.pixelSize: 15
            color: Theme.textPrimary
            text: qsTr("任务执行中，请稍候…")
        }
    }

    Component.onCompleted: {
        // 启动校验自定义壁纸：选择失效时回退纯色（必要时采纳托管目录内的唯一残留）
        Theme.setWallpaper(Wallpaper.reconcile(Theme.wallpaper))

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
