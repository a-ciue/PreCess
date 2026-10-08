import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import "AlgorithmNavigation.js" as AlgorithmNavigation

import app.core
import app.model
import app.model.systems
import app.model.systems.algo
import app.model.systems.edit

ColumnLayout {
    id: root
    spacing: 0

    // 冻结任务在飞（算法执行 ∨ 功能冻结任务）禁用工具栏：inline 执行中不得触发运行/编辑等模型操作
    enabled: !QModelManager.taskStatus.frozenBusy

    readonly property bool isWasm: Qt.platform.os === "wasm"

    property int activeCategory: -1
    property var algorithmSystem: QModelManager.algorithmSystem
    readonly property var algorithmCategories: root.algorithmSystem.navigationCategories
    readonly property var algorithmInfos: root.algorithmSystem.algorithmsInfo
    readonly property int algorithmPageStart: 2
    readonly property int featurePageStart: algorithmPageStart + 2
    property real windowHeight: 600

    // 限制页高范围，给小窗口保留文字空间，避免大窗口工具栏过度放大。
    readonly property real ribbonPageHeight: Math.max(64, Math.min(96, windowHeight / 12))
    // 从独立尺寸推导图标，避免依赖布局分配后的按钮高度形成反馈环。
    readonly property real ribbonIconSize: Math.round(ribbonPageHeight * 0.5)

    signal resetLayoutRequested()
    signal objectTreeToggled()
    signal propertyListToggled()
    signal attributeRenderToggled()
    signal consoleToggled()
    signal pythonConsoleToggled()
    signal outputLogToggled()
    signal preferencesToggled()

    property bool objectTreeOpen: false
    property bool propertyListOpen: false
    property bool attributeRenderOpen: false
    property bool consoleOpen: false
    property bool pythonConsoleOpen: false
    property bool outputLogOpen: false
    property bool preferencesOpen: false

    // 算法与编辑系统尚无图标声明字段，暂保留其名称映射。
    readonly property var pluginIconMap: ({
        "CreateFacePlugin": "qrc:/images/toolbar/Edit/create-face.svg",
        "DeleteFacePlugin": "qrc:/images/toolbar/Edit/delete-face.svg",
        "TetGenPlugin": "qrc:/images/toolbar/Algorithm/tetgen.svg",
        "TetGenLibPlugin": "qrc:/images/toolbar/Algorithm/tetgen.svg",
        "GmshPlugin": "qrc:/images/toolbar/Algorithm/gmsh.svg",
        "cmdExecutePlugin": "qrc:/images/toolbar/Algorithm/cmd.svg"
    })
    function getIconForPlugin(pluginName) {
        return pluginIconMap[pluginName] || "qrc:/images/toolbar/precess_extra_plugin.svg"
    }

    // 功能图标由菜单声明提供，未指定时统一使用通用插件图标。
    function getIconForFeature(info) {
        return info.icon || "qrc:/images/toolbar/precess_extra_plugin.svg"
    }

    function activatePlugin(systemList, pluginName, system) {
        for (var i = 0; i < systemList.length; i++) {
            if (systemList[i].name === pluginName) {
                App.activeOperation = {
                    info: systemList[i],
                    execute: function(model, args) { system.call(pluginName, model, args) }
                };
                break;
            }
        }
    }

    // 导出默认文件名：活动模型名（导入模型即为导入文件名，自带扩展名）。
    // 扩展名交给对话框按所选文件类型适配（Windows IFileDialog 会随类型切换改写），此处不推导
    function suggestedExportFileName() {
        const model_id = App.selection.activeModelId
        return model_id < 0 ? "" : QModelManager.query.getModelName(model_id)
    }

    // 功能触发入口：ribbon 功能按钮共用
    function activateFeature(info) {
        App.activeOperation = {
            info: info,
            isFeature: true,
            execute: function() { return QModelManager.featureSystem.invoke(info.name) }
        }
    }

    // 功能 ribbon 结构：[{ name: 菜单名, groups: [{ name: 分组名, items: [QFeatureInfo] }] }]，由 rebuildFeatureMenus 维护
    property var featureMenus: []

    // 按 menu_path 两级（菜单/分组）重建功能 ribbon 结构（功能注册/注销时调用）
    function rebuildFeatureMenus() {
        let menu_order = []
        let menus = {} // 菜单名 -> { group_order, groups: 分组名 -> [QFeatureInfo] }
        for (let info of QModelManager.featureSystem.featuresInfo) {
            let segs = (info.menu_path || "功能").split('/')
            let menu_name = segs[0]
            let group_name = segs.length > 1 ? segs[1] : ""
            if (!menus[menu_name]) {
                menus[menu_name] = { group_order: [], groups: {} }
                menu_order.push(menu_name)
            }
            let menu = menus[menu_name]
            if (!menu.groups[group_name]) {
                menu.groups[group_name] = []
                menu.group_order.push(group_name)
            }
            menu.groups[group_name].push(info)
        }
        let ribbon = []
        for (let menu_name of menu_order) {
            let menu = menus[menu_name]
            let groups = []
            for (let group_name of menu.group_order)
                groups.push({ name: group_name, items: menu.groups[group_name] })
            ribbon.push({ name: menu_name, groups: groups })
        }
        featureMenus = ribbon
    }

    Connections {
        target: QModelManager.featureSystem
        function onFeaturesInfoChanged() {
            root.rebuildFeatureMenus()
        }
    }

    Component.onCompleted: rebuildFeatureMenus()

    // 网页端导入模型
    Connections {
        target: root.isWasm ? QWasmBridge : null
        function onFilesImported(paths) {
            for (var i = 0; i < paths.length; ++i) {
                QModelManager.ioSystem.read("All files", "file://" + paths[i], [])
            }
            App.registry.renderWindow.resetCamera()
        }
    }

    // 导入模型对话框（支持多选，逐个导入所选文件）
    FileDialog {
        id: importModelDialog
        nameFilters: QModelManager.ioSystem.dialogNameFilters
        fileMode: FileDialog.OpenFiles
        onAccepted: {
            if (selectedNameFilter.index >= 0) {
                for (const file of selectedFiles) {
                    QModelManager.ioSystem.read(selectedNameFilter.name, file, [])
                }
                App.registry.renderWindow.resetCamera()
            } else {
                console.exception("No valid file type selected.")
            }
        }
    }

    // 导出模型对话框
    FileDialog {
        id: exportModelDialog
        nameFilters: QModelManager.ioSystem.dialogNameFilters
        fileMode: FileDialog.SaveFile

        // 以活动模型名预填文件名，避免用户从空白输入框开始；扩展名由对话框按所选类型适配
        function openExport() {
            const name = root.suggestedExportFileName()
            if (name !== "")
                selectedFile = currentFolder + "/" + name
            open()
        }

        onAccepted: {
            if (selectedNameFilter.index >= 0) {
                QModelManager.ioSystem.write(selectedNameFilter.name, App.selection.activeModelId, selectedFile, [])
            } else {
                console.exception("No valid file type selected.")
            }
        }
    }

    // 网页端导出模型
    Popup {
        id: wasmExportDialog
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 380
        padding: Theme.spacingMd
        background: Rectangle {
            color: Theme.surface
            border.color: Theme.border
            radius: Theme.radiusMenu
        }

        function openDialog() {
            typeCombo.model = QModelManager.ioSystem.getModelIOInfo()
            typeCombo.currentIndex = 0
            // 按活动模型名预填（无活动模型时保留既有默认值）；扩展名由类型下拉的联动逻辑适配
            const suggested = root.suggestedExportFileName()
            if (suggested !== "")
                nameField.text = suggested
            open()
        }

        function confirmExport() {
            var info = typeCombo.model[typeCombo.currentIndex]
            if (!info) return
            var file_name = nameField.text.trim()
            if (file_name.length === 0) return
            if (file_name.indexOf(".") < 0 && info.extensions.length > 0)
                file_name += "." + info.extensions[0]
            var path = "/tmp/" + file_name
            QModelManager.ioSystem.write(info.name, App.selection.activeModelId, "file://" + path, [])
            QWasmBridge.downloadFile(file_name, path)
            close()
        }

        ColumnLayout {
            width: parent.width
            spacing: 8

            Label { text: "导出模型" }

            RowLayout {
                Layout.fillWidth: true
                Label { text: "类型:" }
                ComboBox {
                    id: typeCombo
                    Layout.fillWidth: true
                    textRole: "name"
                    onCurrentIndexChanged: {
                        var info = typeCombo.model ? typeCombo.model[typeCombo.currentIndex] : null
                        if (!info || !nameField) return
                        var ext = info.extensions.length > 0 ? "." + info.extensions[0] : ""
                        var base = nameField.text.split(".")[0]
                        if (base.length > 0) nameField.text = base + ext
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Label { text: "文件名:" }
                TextField {
                    id: nameField
                    Layout.fillWidth: true
                    text: "model.obj"
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: "取消"
                    onClicked: wasmExportDialog.close()
                }
                Button {
                    text: "下载"
                    highlighted: true
                    onClicked: wasmExportDialog.confirmExport()
                }
            }
        }
    }

    ToolBar {
        Layout.fillWidth: true
        background: Rectangle {
            color: Theme.surfaceAlt

            // 底部细线分隔页签行与页面内容
            Rectangle {
                anchors {
                    left: parent.left
                    right: parent.right
                    bottom: parent.bottom
                }
                height: 1
                color: Theme.border
            }
        }

        Flickable {
            anchors.fill: parent
            implicitHeight: navigationRow.implicitHeight
            contentWidth: navigationRow.implicitWidth
            contentHeight: height
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            RowLayout {
                id: navigationRow
                spacing: 2

                RibbonTabButton {
                    text: "文件"
                    checkable: true
                    checked: activeCategory === 0
                    onClicked: activeCategory = (activeCategory === 0) ? -1 : 0
                }

                RibbonTabButton {
                    objectName: "editCategory"
                    text: qsTr("编辑")
                    checkable: true
                    checked: activeCategory === 1
                    onClicked: activeCategory = (activeCategory === 1) ? -1 : 1
                }

                RibbonTabButton {
                    objectName: "meshGenerationTab"
                    text: qsTr("网格生成算法")
                    checkable: true
                    checked: root.activeCategory === root.algorithmPageStart
                    onClicked: root.activeCategory = checked ? root.algorithmPageStart : -1
                }

                RibbonTabButton {
                    text: qsTr("其他算法")
                    checkable: true
                    checked: activeCategory === root.featurePageStart - 1
                    onClicked: activeCategory = checked ? root.featurePageStart - 1 : -1
                }

                // 功能菜单页排在固定算法分类页之后
                Repeater {
                    model: root.featureMenus
                    RibbonTabButton {
                        required property var modelData
                        required property int index
                        text: modelData.name
                        checkable: true
                        checked: activeCategory === root.featurePageStart + index
                        onClicked: activeCategory = checked ? root.featurePageStart + index : -1
                    }
                }

                RibbonTabButton {
                    id: viewBtn
                    text: "视图"
                    onClicked: viewMenu.popup(viewBtn, 0, viewBtn.height)
                    Menu {
                        id: viewMenu
                        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

                        Action {
                            text: "对象树"
                            checkable: true
                            checked: objectTreeOpen
                            onToggled: objectTreeToggled()
                        }
                        Action {
                            text: "操作面板"
                            checkable: true
                            checked: propertyListOpen
                            onToggled: propertyListToggled()
                        }
                        Action {
                            text: "属性渲染"
                            checkable: true
                            checked: attributeRenderOpen
                            onToggled: attributeRenderToggled()
                        }
                        Action {
                            text: "JavaScript 控制台"
                            checkable: true
                            checked: consoleOpen
                            onToggled: consoleToggled()
                        }
                        Action {
                            text: "Python 控制台"
                            checkable: true
                            checked: pythonConsoleOpen
                            onToggled: pythonConsoleToggled()
                        }
                        Action {
                            text: "日志"
                            checkable: true
                            checked: outputLogOpen
                            onToggled: outputLogToggled()
                        }
                        Action {
                            text: "偏好设置"
                            checkable: true
                            checked: preferencesOpen
                            onToggled: preferencesToggled()
                        }
                        MenuSeparator {}
                        Action {
                            text: qsTr("恢复默认布局")
                            onTriggered: root.resetLayoutRequested()
                        }
                    }
                }
            }
        }
    }

    StackLayout {
        Layout.fillWidth: true
        Layout.preferredHeight: root.ribbonPageHeight
        implicitHeight: activeCategory >= 0 ? root.ribbonPageHeight : 0
        visible: activeCategory >= 0

        currentIndex: activeCategory

        // 0: 文件
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 2

            RibbonActionButton {
                icon.source: "qrc:/images/toolbar/File/import.svg"
                icon.width: root.ribbonIconSize
                icon.height: root.ribbonIconSize
                icon.color: "transparent"
                Layout.fillHeight: true
                text: "导入"
                onClicked: {
                    if (root.isWasm)
                        QWasmBridge.pickFile(QModelManager.ioSystem.getDialogExtFilters(), true)
                    else
                        importModelDialog.open()
                }
            }

            RibbonActionButton {
                icon.source: "qrc:/images/toolbar/File/export.svg"
                icon.width: root.ribbonIconSize
                icon.height: root.ribbonIconSize
                icon.color: "transparent"
                Layout.fillHeight: true
                text: "导出"
                onClicked: {
                    if (root.isWasm)
                        wasmExportDialog.openDialog()
                    else
                        exportModelDialog.openExport()
                }
            }

            RibbonActionButton {
                icon.source: "qrc:/images/toolbar/File/preference.svg"
                icon.width: root.ribbonIconSize
                icon.height: root.ribbonIconSize
                icon.color: "transparent"
                Layout.fillHeight: true
                text: "偏好设置"
                onClicked: preferencesToggled()
            }

            Item { Layout.fillWidth: true }
        }

        // 1: 编辑 → 撤销/重做 + 数据驱动，图标按名映射
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 2

            RibbonActionButton {
                icon.source: "qrc:/images/toolbar/Common/undo.svg"
                icon.width: root.ribbonIconSize
                icon.height: root.ribbonIconSize
                icon.color: "transparent"
                text: qsTr("撤销")
                enabled: QModelManager.undoStack.canUndo
                Layout.fillHeight: true
                ToolTip.visible: hovered
                ToolTip.text: QModelManager.undoStack.undoLabel.length > 0
                              ? qsTr("撤销 ") + QModelManager.undoStack.undoLabel
                              : qsTr("撤销")
                onClicked: QModelManager.undoStack.undo()
            }

            RibbonActionButton {
                icon.source: "qrc:/images/toolbar/Common/redo.svg"
                icon.width: root.ribbonIconSize
                icon.height: root.ribbonIconSize
                icon.color: "transparent"
                text: qsTr("重做")
                enabled: QModelManager.undoStack.canRedo
                Layout.fillHeight: true
                ToolTip.visible: hovered
                ToolTip.text: QModelManager.undoStack.redoLabel.length > 0
                              ? qsTr("重做 ") + QModelManager.undoStack.redoLabel
                              : qsTr("重做")
                onClicked: QModelManager.undoStack.redo()
            }

            ToolSeparator {
                orientation: Qt.Vertical
                Layout.fillHeight: true
            }

            Repeater {
                model: QModelManager.editSystem.editsInfo
                RibbonActionButton {
                    required property var modelData
                    icon.source: root.getIconForPlugin(modelData.name)
                    icon.width: root.ribbonIconSize
                    icon.height: root.ribbonIconSize
                    icon.color: "transparent"
                    Layout.fillHeight: true
                    text: modelData.display_name
                    onClicked: {
                        App.activeOperation = {
                            info: modelData,
                            execute: function(model, args) {
                                QModelManager.editSystem.call(modelData.name, model, args)
                                // Edit 操作可能改变网格拓扑，原有单元 ID 不再可靠。
                                App.selection.selectionInvalidated()
                            }
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }

        Flickable {
            id: meshCategoryPage
            objectName: "meshCategoryPage"
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: meshCategoryRow.implicitWidth
            contentHeight: height
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            RowLayout {
                id: meshCategoryRow
                height: meshCategoryPage.height
                spacing: 2
                Repeater {
                    model: root.algorithmCategories
                    RibbonActionButton {
                        required property var modelData
                        objectName: "algorithmCategory_" + modelData.id
                        text: modelData.title
                        icon.source: modelData.icon || "qrc:/images/toolbar/precess_extra_plugin.svg"
                        icon.width: root.ribbonIconSize
                        icon.height: root.ribbonIconSize
                        icon.color: "transparent"
                        Layout.fillHeight: true
                        checkable: true
                        autoExclusive: true
                        checked: !!(App.activeOperation && App.activeOperation.isMeshGeneration
                            && App.activeOperation.meshCategory === modelData.id)
                        onClicked: {
                            if (!root.propertyListOpen)
                                root.propertyListToggled()
                            // 重复选择保留输入；新类别由操作面板解析默认或上次所选算法。
                            if (App.activeOperation && App.activeOperation.isMeshGeneration
                                    && App.activeOperation.meshCategory === modelData.id)
                                return
                            App.activeOperation = {
                                isMeshGeneration: true,
                                meshCategory: modelData.id,
                                categoryTitle: modelData.title,
                                info: null
                            }
                        }
                    }
                }
            }
        }

        // 其他算法沿用原有按钮与参数面板入口。
        Flickable {
            id: algorithmPage
            objectName: "algorithmPage_other"
            readonly property var groups: AlgorithmNavigation.buildGroups(root.algorithmInfos, "other")
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: algorithmGroups.implicitWidth
            contentHeight: height
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            RowLayout {
                id: algorithmGroups
                height: algorithmPage.height
                spacing: Theme.spacingSm

                Repeater {
                    model: algorithmPage.groups
                    ColumnLayout {
                        id: algorithmGroup
                        required property var modelData
                        spacing: 0
                        Layout.fillHeight: true

                        RowLayout {
                            Layout.fillHeight: true
                            spacing: 2
                            Repeater {
                                model: algorithmGroup.modelData.items
                                RibbonActionButton {
                                    id: algorithmAction
                                    required property var modelData
                                    topPadding: 4
                                    bottomPadding: 4
                                    objectName: "algorithmAction_" + modelData.name
                                    icon.source: modelData.icon || "qrc:/images/toolbar/precess_extra_plugin.svg"
                                    // 按钮文字和内边距先占位，图标使用剩余高度。
                                    icon.height: Math.max(16, Math.min(root.ribbonIconSize,
                                        root.ribbonPageHeight - topPadding - bottomPadding
                                        - spacing - actionFontMetrics.height))
                                    icon.width: icon.height
                                    icon.color: "transparent"
                                    Layout.fillHeight: true
                                    text: modelData.display_name
                                    onClicked: root.activatePlugin(root.algorithmInfos, modelData.name, QModelManager.algorithmSystem)
                                    FontMetrics {
                                        id: actionFontMetrics
                                        font: algorithmAction.font
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Label {
                parent: algorithmPage
                anchors.centerIn: parent
                visible: algorithmPage.groups.length === 0
                text: qsTr("暂无可用算法")
                color: Theme.textSecondary
            }
        }

        // 功能菜单页：页内按 menu_path 第二段（分组）排列功能按钮，同组排在一起，组间以竖线分隔
        Repeater {
            model: root.featureMenus
            RowLayout {
                id: featureMenuPage
                required property var modelData
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 2

                Repeater {
                    model: featureMenuPage.modelData.groups
                    RowLayout {
                        id: featureGroupRow
                        required property var modelData
                        required property int index
                        Layout.fillHeight: true
                        spacing: 2

                        Repeater {
                            model: featureGroupRow.modelData.items
                            RibbonActionButton {
                                required property var modelData
                                icon.source: root.getIconForFeature(modelData)
                                icon.width: root.ribbonIconSize
                                icon.height: root.ribbonIconSize
                                icon.color: "transparent"
                                Layout.fillHeight: true
                                text: modelData.display_name
                                onClicked: root.activateFeature(modelData)
                            }
                        }

                        // 分组间竖线分隔（最后一组不显示）
                        ToolSeparator {
                            orientation: Qt.Vertical
                            Layout.fillHeight: true
                            visible: featureGroupRow.index < featureMenuPage.modelData.groups.length - 1
                        }
                    }
                }

                Item { Layout.fillWidth: true }
            }
        }
    }
}
