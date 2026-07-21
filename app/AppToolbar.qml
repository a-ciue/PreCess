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

            ToolButton {
                text: "功能"
                checkable: true
                checked: activeCategory === 3
                onClicked: activeCategory = (activeCategory === 3) ? -1 : 3
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

        // 3: 功能 → 数据驱动，图标按名映射（自定义功能插件使用默认图标）
        RowLayout {
            anchors.fill: parent

            Repeater {
                model: QModelManager.featureSystem.featuresInfo
                ToolButton {
                    required property var modelData
                    icon.source: root.getIconForPlugin(modelData.name)
                    icon.width: parent.height * 0.65
                    icon.height: parent.height * 0.65
                    Layout.fillHeight: true
                    display: ToolButton.TextUnderIcon
                    text: modelData.display_name
                    onClicked: {
                        App.activeOperation = {
                            info: modelData,
                            isFeature: true,
                            execute: function() { return QModelManager.featureSystem.invoke(modelData.name) }
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }
    }
}
