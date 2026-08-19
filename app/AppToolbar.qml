import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import app.core
import app.model
import app.model.systems
import app.model.systems.algo
import app.model.systems.edit

ColumnLayout {
    id: root
    spacing: 0

    property int activeCategory: -1
    property real windowHeight: 600

    // ribbon 页内按钮图标尺寸。页高即 windowHeight/12；图标若绑定 parent.height，
    // 会与布局隐式尺寸形成反馈环（图标异步加载后触发 recursive rearrange 警告）。
    readonly property real ribbonIconSize: windowHeight / 12 * 0.65

    // 基础几何创建入口，由 Main.qml 中的 GeometryOperationActions 提供。
    property var geometryOperationActions

    signal objectTreeToggled()
    signal propertyListToggled()
    signal attributeRenderToggled()
    signal consoleToggled()
    signal outputLogToggled()
    signal preferencesToggled()

    property bool objectTreeOpen: false
    property bool propertyListOpen: false
    property bool attributeRenderOpen: false
    property bool consoleOpen: false
    property bool outputLogOpen: false
    property bool preferencesOpen: false

    // 已知插件名 → 图标映射，未命中则使用 PreCess_extra_plugin.svg
    readonly property var pluginIconMap: ({
        "CreateFacePlugin": "qrc:/images/toolbar/Edit/create_face.svg",
        "DeleteFacePlugin": "qrc:/images/toolbar/Edit/delete_face.svg",
        "TetGenPlugin": "qrc:/images/toolbar/Algorithm/tetgen.svg",
        "TetGenLibPlugin": "qrc:/images/toolbar/Algorithm/tetgen.svg",
        "GmshPlugin": "qrc:/images/toolbar/Algorithm/gmsh.svg",
        "cmdExecutePlugin": "qrc:/images/toolbar/Algorithm/cmd.svg",
        "MeasurePlugin": "qrc:/images/toolbar/Tools/measure.svg",
        "DimensionPlugin": "qrc:/images/toolbar/Tools/size_marking.svg",
        "MeshQuality": "qrc:/images/toolbar/Function/grid_quality.svg"
    })
    // 几何页按钮定义：当前使用默认插件图标。
    readonly property var geometryOperationButtons: [
        { text: qsTr("点"), operation: "startCreatePoint",
          icon: "qrc:/images/toolbar/Geometry/point.svg" },
        { text: qsTr("直线边（坐标）"), operation: "startCreateLineByCoordinates",
          icon: "qrc:/images/toolbar/Geometry/line_coord.svg" },
        { text: qsTr("直线边（选点）"), operation: "startCreateLineFromVertices",
          icon: "qrc:/images/toolbar/Geometry/line_points.svg" },
        { text: qsTr("矩形面"), operation: "startCreateRectangleFace",
          icon: "qrc:/images/toolbar/Geometry/rectangle.svg" },
        { text: qsTr("圆盘/扇形面"), operation: "startCreateDiskFace",
          icon: "qrc:/images/toolbar/Geometry/sector_or_circle.svg" },
        { text: qsTr("闭合边成面"), operation: "startCreateFaceFromEdges",
          icon: "qrc:/images/toolbar/Geometry/close_edges_to_form_surface.svg" },
        { text: qsTr("长方体"), operation: "startCreateBox",
          icon: "qrc:/images/toolbar/Geometry/cuboid.svg" },
        { text: qsTr("圆柱体"), operation: "startCreateCylinder",
          icon: "qrc:/images/toolbar/Geometry/cylinder.svg" },
        { text: qsTr("圆锥/圆台"), operation: "startCreateCone",
          icon: "qrc:/images/toolbar/Geometry/cone_or_conical_stage.svg" },
        { text: qsTr("球体/部分球体"), operation: "startCreateSphere",
          icon: "qrc:/images/toolbar/Geometry/sphere.svg" },
        { text: qsTr("拉伸面"), operation: "startExtrudeFace",
          icon: "qrc:/images/toolbar/Geometry/stretched_surface.svg" },
        { text: qsTr("删除几何"), operation: "startDeleteGeometry",
          icon: "qrc:/images/toolbar/Geometry/delete_geometry.svg" }
    ]

    function getIconForPlugin(pluginName) {
        return pluginIconMap[pluginName] || "qrc:/images/toolbar/precess_extra_plugin.svg"
    }

    // 功能图标：优先菜单声明的自定义 qrc 图标，未指定时按插件名映射默认图标
    function getIconForFeature(info) {
        return info.icon ? info.icon : getIconForPlugin(info.name)
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
        onAccepted: {
            if (selectedNameFilter.index >= 0) {
                QModelManager.ioSystem.write(selectedNameFilter.name, App.selection.activeModelId, selectedFile, [])
            } else {
                console.exception("No valid file type selected.")
            }
        }
    }

    ToolBar {
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            ToolButton {
                text: "文件"
                checkable: true
                checked: activeCategory === 0
                onClicked: activeCategory = (activeCategory === 0) ? -1 : 0
            }

            ToolButton {
                text: qsTr("编辑")
                checkable: true
                checked: activeCategory === 1
                onClicked: activeCategory = (activeCategory === 1) ? -1 : 1
            }

            ToolButton {
                text: qsTr("算法")
                checkable: true
                checked: activeCategory === 2
                onClicked: activeCategory = (activeCategory === 2) ? -1 : 2
            }

            ToolButton {
                text: qsTr("几何")
                checkable: true
                checked: activeCategory === 3
                onClicked: activeCategory = (activeCategory === 3) ? -1 : 3
            }

            // 功能菜单分页：按 menu_path 第一段（菜单）动态生成分页按钮，页序对应 StackLayout 索引 4 起
            Repeater {
                model: root.featureMenus
                ToolButton {
                    required property var modelData
                    required property int index
                    text: modelData.name
                    checkable: true
                    checked: activeCategory === 4 + index
                    onClicked: activeCategory = (activeCategory === 4 + index) ? -1 : 4 + index
                }
            }

            ToolButton {
                id: viewBtn
                text: "视图"
                onClicked: viewMenu.popup(viewBtn, 0, viewBtn.height)
                Menu {
                    id: viewMenu
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
                    topPadding: 2
                    bottomPadding: 2

                    delegate: MenuItem {
                        implicitHeight: 22
                        padding: 0
                        leftPadding: 6
                        rightPadding: 6
                        spacing: 0
                    }

                    Action {
                        text: "对象树"
                        checkable: true
                        checked: objectTreeOpen
                        onToggled: objectTreeToggled()
                    }
                    Action {
                        text: "属性列表"
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
                        text: "控制台"
                        checkable: true
                        checked: consoleOpen
                        onToggled: consoleToggled()
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
                }
            }

            Item { Layout.fillWidth: true }
        }
    }

    StackLayout {
        implicitHeight: activeCategory >= 0 ? windowHeight / 12 : 0
        visible: activeCategory >= 0

        currentIndex: activeCategory

        // 0: 文件
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ToolButton {
                icon.source: "qrc:/images/toolbar/File/import.svg"
                icon.width: root.ribbonIconSize
                icon.height: root.ribbonIconSize
                icon.color: "transparent"
                Layout.fillHeight: true
                display: ToolButton.TextUnderIcon
                text: "导入"
                onClicked: importModelDialog.open()
            }

            ToolButton {
                icon.source: "qrc:/images/toolbar/File/export.svg"
                icon.width: root.ribbonIconSize
                icon.height: root.ribbonIconSize
                icon.color: "transparent"
                Layout.fillHeight: true
                display: ToolButton.TextUnderIcon
                text: "导出"
                onClicked: exportModelDialog.open()
            }

            ToolButton {
                icon.source: "qrc:/images/toolbar/File/preference.svg"
                icon.width: root.ribbonIconSize
                icon.height: root.ribbonIconSize
                icon.color: "transparent"
                Layout.fillHeight: true
                display: ToolButton.TextUnderIcon
                text: "偏好设置"
                onClicked: preferencesToggled()
            }

            Item { Layout.fillWidth: true }
        }

        // 1: 编辑 → 数据驱动，图标按名映射
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Repeater {
                model: QModelManager.editSystem.editsInfo
                ToolButton {
                    required property var modelData
                    icon.source: root.getIconForPlugin(modelData.name)
                    icon.width: root.ribbonIconSize
                    icon.height: root.ribbonIconSize
                    icon.color: "transparent"
                    Layout.fillHeight: true
                    display: ToolButton.TextUnderIcon
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

        // 2: 算法 → 数据驱动，图标按名映射
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Repeater {
                model: QModelManager.algorithmSystem.algorithmsInfo
                ToolButton {
                    required property var modelData
                    icon.source: root.getIconForPlugin(modelData.name)
                    icon.width: root.ribbonIconSize
                    icon.height: root.ribbonIconSize
                    icon.color: "transparent"
                    Layout.fillHeight: true
                    display: ToolButton.TextUnderIcon
                    text: modelData.display_name
                    onClicked: root.activatePlugin(QModelManager.algorithmSystem.algorithmsInfo, modelData.name, QModelManager.algorithmSystem)
                }
            }

            Item { Layout.fillWidth: true }
        }

        // 3: 几何 → 创建几何。
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Repeater {
                model: root.geometryOperationButtons
                ToolButton {
                    required property var modelData
                    icon.source: root.getIconForFeature(modelData)
                    icon.width: root.ribbonIconSize
                    icon.height: root.ribbonIconSize
                    icon.color: "transparent"
                    Layout.fillHeight: true
                    display: ToolButton.TextUnderIcon
                    text: modelData.text
                    onClicked: root.geometryOperationActions[modelData.operation]()
                }
            }

            Item { Layout.fillWidth: true }
        }

        // 功能菜单页（索引 4 起）：页内按 menu_path 第二段（分组）排列功能按钮，同组排在一起，组间以竖线分隔
        Repeater {
            model: root.featureMenus
            RowLayout {
                id: featureMenuPage
                required property var modelData
                Layout.fillWidth: true
                Layout.fillHeight: true

                Repeater {
                    model: featureMenuPage.modelData.groups
                    RowLayout {
                        id: featureGroupRow
                        required property var modelData
                        required property int index
                        Layout.fillHeight: true
                        spacing: 0

                        Repeater {
                            model: featureGroupRow.modelData.items
                            ToolButton {
                                required property var modelData
                                icon.source: root.getIconForFeature(modelData)
                                icon.width: root.ribbonIconSize
                                icon.height: root.ribbonIconSize
                                icon.color: "transparent"
                                Layout.fillHeight: true
                                display: ToolButton.TextUnderIcon
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
