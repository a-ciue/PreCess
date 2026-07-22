import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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

    signal importRequested()
    signal exportRequested()
    signal objectTreeToggled()
    signal propertyListToggled()
    signal consoleToggled()
    signal outputLogToggled()
    signal preferencesToggled()

    property bool objectTreeOpen: false
    property bool propertyListOpen: false
    property bool consoleOpen: false
    property bool outputLogOpen: false
    property bool preferencesOpen: false

    // 已知插件名 → 图标映射，未命中则使用 PreCess_extra_plugin.svg
    readonly property var pluginIconMap: ({
        "CreateFacePlugin": "qrc:/images/toolbar/create_face.svg",
        "DeleteFacePlugin": "qrc:/images/toolbar/delete_face.svg",
        "TetGenPlugin": "qrc:/images/toolbar/TetGen.svg",
        "TetGenLibPlugin": "qrc:/images/toolbar/TetGen.svg",
        "GmshPlugin": "qrc:/images/toolbar/gmsh.svg",
        "cmdExecutePlugin": "qrc:/images/toolbar/cmd.svg"
    })

    function getIconForPlugin(pluginName) {
        return pluginIconMap[pluginName] || "qrc:/images/toolbar/PreCess_extra_plugin.svg"
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

    ToolBar {
        RowLayout {
            anchors.fill: parent
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

            // 功能菜单分页：按 menu_path 第一段（菜单）动态生成分页按钮，页序对应 StackLayout 索引 3 起
            Repeater {
                model: root.featureMenus
                ToolButton {
                    required property var modelData
                    required property int index
                    text: modelData.name
                    checkable: true
                    checked: activeCategory === 3 + index
                    onClicked: activeCategory = (activeCategory === 3 + index) ? -1 : 3 + index
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
            anchors.fill: parent

            ToolButton {
                icon.source: "qrc:/images/toolbar/import.svg"
                icon.width: parent.height * 0.65
                icon.height: parent.height * 0.65
                Layout.fillHeight: true
                display: ToolButton.TextUnderIcon
                text: "导入"
                onClicked: importRequested()
            }

            ToolButton {
                icon.source: "qrc:/images/toolbar/export.svg"
                icon.width: parent.height * 0.65
                icon.height: parent.height * 0.65
                Layout.fillHeight: true
                display: ToolButton.TextUnderIcon
                text: "导出"
                onClicked: exportRequested()
            }

            ToolButton {
                icon.source: "qrc:/images/toolbar/preference.svg"
                icon.width: parent.height * 0.65
                icon.height: parent.height * 0.65
                Layout.fillHeight: true
                display: ToolButton.TextUnderIcon
                text: "偏好设置"
                onClicked: preferencesToggled()
            }

            Item { Layout.fillWidth: true }
        }

        // 1: 编辑 → 数据驱动，图标按名映射
        RowLayout {
            anchors.fill: parent

            Repeater {
                model: QModelManager.editSystem.editsInfo
                ToolButton {
                    required property var modelData
                    icon.source: root.getIconForPlugin(modelData.name)
                    icon.width: parent.height * 0.65
                    icon.height: parent.height * 0.65
                    Layout.fillHeight: true
                    display: ToolButton.TextUnderIcon
                    text: modelData.display_name
                    onClicked: root.activatePlugin(QModelManager.editSystem.editsInfo, modelData.name, QModelManager.editSystem)
                }
            }

            Item { Layout.fillWidth: true }
        }

        // 2: 算法 → 数据驱动，图标按名映射
        RowLayout {
            anchors.fill: parent

            Repeater {
                model: QModelManager.algorithmSystem.algorithmsInfo
                ToolButton {
                    required property var modelData
                    icon.source: root.getIconForPlugin(modelData.name)
                    icon.width: parent.height * 0.65
                    icon.height: parent.height * 0.65
                    Layout.fillHeight: true
                    display: ToolButton.TextUnderIcon
                    text: modelData.display_name
                    onClicked: root.activatePlugin(QModelManager.algorithmSystem.algorithmsInfo, modelData.name, QModelManager.algorithmSystem)
                }
            }

            Item { Layout.fillWidth: true }
        }

        // 功能菜单页（索引 3 起）：页内按 menu_path 第二段（分组）排列功能按钮，同组排在一起，组间以竖线分隔
        Repeater {
            model: root.featureMenus
            RowLayout {
                id: featureMenuPage
                required property var modelData
                anchors.fill: parent

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
                                icon.width: parent.height * 0.65
                                icon.height: parent.height * 0.65
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
