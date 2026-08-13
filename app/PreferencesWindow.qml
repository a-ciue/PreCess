import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import app.model

Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.preferredWidth: 120
                Layout.fillHeight: true
                color: "#f0f0f0"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 4
                    spacing: 2

                    ButtonGroup {
                        id: categoryGroup
                    }

                    ToolButton {
                        text: "主题"
                        Layout.fillWidth: true
                        checkable: true
                        checked: true
                        ButtonGroup.group: categoryGroup
                        onClicked: panelStack.currentIndex = 0
                    }

                    ToolButton {
                        text: "插件"
                        Layout.fillWidth: true
                        checkable: true
                        ButtonGroup.group: categoryGroup
                        onClicked: panelStack.currentIndex = 1
                    }

                    Item { Layout.fillHeight: true }
                }
            }

            ToolSeparator {
                Layout.fillHeight: true
            }

            StackLayout {
                id: panelStack
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 8
                currentIndex: 0

                Item {
                    Label {
                        anchors.centerIn: parent
                        text: "主题设置 - 待开发"
                        color: "#888"
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: pluginListView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: QModelManager.systemPluginManager.pluginNames
                        currentIndex: -1

                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 30
                            color: pluginListView.currentIndex === index ? "gray" : "white"
                            border.color: "#ccc"
                            border.width: 1

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                anchors.right: parent.right
                                anchors.rightMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData
                                elide: Text.ElideRight
                                color: pluginListView.currentIndex === index ? "white" : "black"
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (pluginListView.currentIndex === index) {
                                        pluginListView.currentIndex = -1
                                    } else {
                                        pluginListView.currentIndex = index
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        ToolSeparator {
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            visible: panelStack.currentIndex === 1

            Item { Layout.fillWidth: true }

            Button {
                text: "注册"
                implicitWidth: 70
                implicitHeight: 26
                onClicked: pluginFileDialog.open()
            }

            Button {
                text: "注销"
                implicitWidth: 70
                implicitHeight: 26
                enabled: pluginListView.currentIndex !== -1
                onClicked: {
                    var selectedPlugin = QModelManager.systemPluginManager.pluginNames[pluginListView.currentIndex]
                    var pluginPath = QModelManager.systemPluginManager.getPluginPath(selectedPlugin)
                    QModelManager.systemPluginManager.unregisterPlugin(pluginPath)
                    pluginListView.currentIndex = -1
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: panelStack.currentIndex === 0

            Item { Layout.fillWidth: true }

            Button {
                text: "恢复默认"
                implicitWidth: 70
                implicitHeight: 26
            }

            Button {
                text: "应用"
                implicitWidth: 70
                implicitHeight: 26
            }
        }
    }

    FileDialog {
        id: pluginFileDialog
        title: "请选择插件文件"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Plugin files (*.dll *.so)", "All files (*)"]

        onAccepted: {
            var result = QModelManager.systemPluginManager.registerPlugin(selectedFile)
            if (!result) {
                registerFailureDialog.open()
            }
        }
    }

    Dialog {
        id: registerFailureDialog
        title: "注册失败"
        standardButtons: Dialog.Ok
        modal: true
        anchors.centerIn: parent

        Label {
            text: "插件注册失败"
        }
    }
}
