import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import app.model

Item {
    id: pluginWindow

    ColumnLayout {
        anchors.fill: parent
        
        ListView {
            id: pluginListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: QModelManager.systemPluginManager.pluginNames
            currentIndex: -1

            delegate: Rectangle {
                id: pluginItem
                width: pluginListView.width
                height: Math.max(30, implicitHeight)
                color: pluginListView.currentIndex === index ? "lightblue" : "transparent"
                border.color: "lightgray"
                border.width: 1
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData
                    color: "black"
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
        
        RowLayout {
            Layout.fillWidth: true
            
            Button {
                text: "添加"
                onClicked: pluginFileDialog.open()
            }
            
            Button {
                text: "注销"
                enabled: pluginListView.currentIndex !== -1
                onClicked: {
                    let selectedPlugin = QModelManager.systemPluginManager.pluginNames[pluginListView.currentIndex]
                    let pluginPath = QModelManager.systemPluginManager.getPluginPath(selectedPlugin)
                    QModelManager.systemPluginManager.unregisterPlugin(pluginPath)
                    pluginListView.currentIndex = -1
                }
            }
            
            Button {
                text: "取消"
                onClicked: pluginManagerDialog.close()
            }
        }
    }
    
    FileDialog {
        id: pluginFileDialog
        title: "请选择插件文件"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Plugin files (*.dll *.so)", "All files (*)"]
        
        onAccepted: {
            let result = QModelManager.systemPluginManager.registerPlugin(selectedFile)
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
