import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import app.core
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
                color: Theme.surfaceAlt

                // 侧栏与内容区的细线分隔
                Rectangle {
                    anchors {
                        top: parent.top
                        bottom: parent.bottom
                        right: parent.right
                    }
                    width: 1
                    color: Theme.border
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingXs
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

                    Item {
                        Layout.fillHeight: true
                    }
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

                ScrollView {
                    id: themeScroll
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    // 主题页内容较高，窗口变小时纵向滚动便于查看
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                    clip: true

                    ColumnLayout {
                        width: parent.width
                        spacing: Theme.spacingSm

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingMd

                            Label {
                                text: qsTr("主题")
                                font.pixelSize: Theme.fontSizeLarge
                                color: Theme.textPrimary
                                Layout.fillWidth: true
                            }

                            ComboBox {
                                id: themeCombo
                                Layout.preferredWidth: 160
                                model: [qsTr("浅色"), qsTr("深色")]
                                onActivated: Theme.setMode(currentIndex)
                            }

                            // 用 Binding 元素同步选中项：直接绑定 currentIndex 会被
                            // 下拉选择破坏，外部改主题（如恢复默认）后无法回显
                            Binding {
                                target: themeCombo
                                property: "currentIndex"
                                value: Theme.mode
                            }
                        }

                        // 壁纸：导入一张图片替换主题纯色背景（壁纸叠在主题纯色之上，深浅通用）
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingSm

                            Label {
                                text: qsTr("壁纸")
                                font.pixelSize: Theme.fontSizeLarge
                                color: Theme.textPrimary
                            }

                            RowLayout {
                                spacing: Theme.spacingMd

                                // 预览：有自定义图显示图片，否则显示主题纯色默认
                                Rectangle {
                                    Layout.preferredWidth: 160
                                    Layout.preferredHeight: 90
                                    color: Theme.menuSurface
                                    border.color: Theme.border
                                    border.width: 1
                                    radius: Theme.radiusControl
                                    clip: true

                                    Image {
                                        anchors.fill: parent
                                        anchors.margins: 1
                                        source: Theme.wallpaperSource
                                        fillMode: Image.PreserveAspectCrop
                                        asynchronous: true
                                        visible: source != ""
                                    }

                                    Label {
                                        anchors.centerIn: parent
                                        visible: Theme.wallpaper === ""
                                        text: qsTr("默认（纯色）")
                                        color: Theme.textSecondary
                                        font.pixelSize: Theme.fontSizeCaption
                                    }
                                }

                                ColumnLayout {
                                    spacing: Theme.spacingXs

                                    Button {
                                        text: qsTr("导入图片")
                                        flat: true
                                        implicitHeight: 26
                                        onClicked: wallpaperFileDialog.open()
                                    }

                                    Button {
                                        text: qsTr("清除")
                                        flat: true
                                        implicitHeight: 26
                                        enabled: Theme.wallpaper !== ""
                                        onClicked: {
                                            Wallpaper.removeWallpaper();
                                            Theme.setWallpaper("");
                                        }
                                    }
                                }

                                Item {
                                    Layout.fillWidth: true
                                }
                            }
                        }

                        Item {
                            Layout.fillHeight: true
                        }
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
                            radius: Theme.radiusControl
                            color: pluginListView.currentIndex === index ? Theme.primaryTint : Theme.surface
                            border.color: pluginListView.currentIndex === index ? Theme.primary : Theme.border
                            border.width: 1

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                anchors.right: parent.right
                                anchors.rightMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData
                                elide: Text.ElideRight
                                color: Theme.textPrimary
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (pluginListView.currentIndex === index) {
                                        pluginListView.currentIndex = -1;
                                    } else {
                                        pluginListView.currentIndex = index;
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
            visible: panelStack.currentIndex === 1 && QModelManager.systemPluginManager.canLoadExternalPlugins

            Item {
                Layout.fillWidth: true
            }

            Button {
                icon.source: "qrc:/images/toolbar/Panels/plugin-register.svg"
                icon.width: 16
                icon.height: 16
                icon.color: "transparent"
                text: "注册"
                flat: true
                implicitWidth: 70
                implicitHeight: 26
                onClicked: pluginFileDialog.open()
            }

            Button {
                icon.source: "qrc:/images/toolbar/Panels/plugin-unregister.svg"
                icon.width: 16
                icon.height: 16
                icon.color: "transparent"
                text: "注销"
                flat: true
                implicitWidth: 70
                implicitHeight: 26
                enabled: pluginListView.currentIndex !== -1
                onClicked: {
                    var selectedPlugin = QModelManager.systemPluginManager.pluginNames[pluginListView.currentIndex];
                    var pluginPath = QModelManager.systemPluginManager.getPluginPath(selectedPlugin);
                    QModelManager.systemPluginManager.unregisterPlugin(pluginPath);
                    pluginListView.currentIndex = -1;
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: panelStack.currentIndex === 0

            Item {
                Layout.fillWidth: true
            }

            Button {
                icon.source: "qrc:/images/toolbar/Common/restore-defaults.svg"
                icon.width: 16
                icon.height: 16
                icon.color: "transparent"
                text: "恢复默认"
                flat: true
                implicitHeight: 26
                onClicked: Theme.setMode(Theme.Mode.Light)
            }

            Button {
                text: "应用"
                flat: true
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
            var result = QModelManager.systemPluginManager.registerPlugin(selectedFile);
            if (!result) {
                registerFailureDialog.open();
            }
        }
    }

    Dialog {
        id: registerFailureDialog
        title: "注册失败"
        standardButtons: Dialog.Ok
        modal: true
        anchors.centerIn: parent

        // 显式绑定 Theme 令牌：对话框弹窗按创建时快照取窗口 palette，
        // 直接绑定 Theme 才能跟随主题切换；弹窗用不透明 menuSurface 保证可读
        palette.window: Theme.menuSurface
        palette.base: Theme.menuSurface
        palette.windowText: Theme.textPrimary
        palette.text: Theme.textPrimary
        palette.button: Theme.menuSurface
        palette.buttonText: Theme.textPrimary
        palette.highlight: Theme.primary
        palette.highlightedText: Theme.textOnPrimary
        palette.mid: Theme.borderStrong
        palette.midlight: Theme.border
        palette.dark: Theme.borderStrong
        palette.light: Theme.menuSurface
        palette.shadow: Theme.scrollBarHover
        palette.placeholderText: Theme.textDisabled

        Label {
            text: "插件注册失败"
        }
    }

    // 壁纸导入：选择本地图片，交由 C++ 校验并复制到托管目录
    FileDialog {
        id: wallpaperFileDialog
        title: "请选择背景图片"
        fileMode: FileDialog.OpenFile
        nameFilters: ["图片文件 (*.jpg *.jpeg *.png *.webp *.bmp)", "所有文件 (*)"]

        onAccepted: {
            var id = Wallpaper.importWallpaper(selectedFile);
            if (id.length > 0)
                Theme.setWallpaper(id);
            else
                wallpaperFailureDialog.open();
        }
    }

    Dialog {
        id: wallpaperFailureDialog
        title: "导入失败"
        standardButtons: Dialog.Ok
        modal: true
        anchors.centerIn: parent

        palette.window: Theme.menuSurface
        palette.base: Theme.menuSurface
        palette.windowText: Theme.textPrimary
        palette.text: Theme.textPrimary
        palette.button: Theme.menuSurface
        palette.buttonText: Theme.textPrimary
        palette.highlight: Theme.primary
        palette.highlightedText: Theme.textOnPrimary
        palette.mid: Theme.borderStrong
        palette.midlight: Theme.border
        palette.dark: Theme.borderStrong
        palette.light: Theme.menuSurface
        palette.shadow: Theme.scrollBarHover
        palette.placeholderText: Theme.textDisabled

        Label {
            text: "无法导入所选图片，请确认是有效的图片文件"
        }
    }
}
