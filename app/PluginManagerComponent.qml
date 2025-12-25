import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Item {
    id: pluginWindow
    
    required property var pluginManager

    Connections {
        target: pluginManager
        onPluginNamesChanged: {
            pluginListView.model = pluginManager.pluginNames
        }
    }

    ColumnLayout {
        anchors.fill: parent
        
        ListView {
            id: pluginListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: pluginManager.pluginNames
            currentIndex: -1

            delegate: Rectangle {
                id: pluginItem
                width: pluginListView.width
                height: 30
                color: pluginListView.currentIndex === index ? "lightblue" : "transparent"
                border.color: "lightgray"
                
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
                    if (pluginListView.currentIndex >= 0 && pluginListView.currentIndex < pluginManager.pluginNames.length) {
                        var selectedPlugin = pluginManager.pluginNames[pluginListView.currentIndex]
                        var pluginPath = pluginManager.getPluginPath(selectedPlugin)
                        pluginManager.unregisterPlugin(pluginPath)
                        pluginListView.currentIndex = -1
                    }
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
            var pluginPath = selectedFile.toString()
            // 移除 "file:///" 前缀
            if (pluginPath.startsWith("file:///")) {
                pluginPath = pluginPath.substring(8)
            }
            var result = pluginManager.registerPlugin(pluginPath)
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
