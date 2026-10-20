/**
 * @file PreferencesWindow.qml
 * @brief 偏好设置：主题与背景、插件注册与卸载
 *
 * 主题页把明暗与壁纸收束到同一张工作区标本上，文案只保留控件名与动作名；
 * 窄面板下分类导航自动转为顶部横向标签，更改即时生效。
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import app.core
import app.model

Item {
    id: root

    // 收窄面板宽度可以保留；高度不足以展示预览时由滚动区承接。
    implicitHeight: 340

    //! @brief 面板窄于该值时，左侧分类栏切换为顶部横向标签
    readonly property bool compact: width < 440
    //! @brief 插件页底栏与分隔线的统一可见条件
    readonly property bool pluginFooterVisible: panelStack.currentIndex === 1 && QModelManager.systemPluginManager.canLoadExternalPlugins

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ---- 窄面板：顶部横向分类标签 ----
        Item {
            visible: root.compact
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 40 : 0

            RowLayout {
                anchors.fill: parent
                spacing: 0

                PreferenceCategoryButton {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    tabMode: true
                    text: qsTr("主题")
                    active: panelStack.currentIndex === 0
                    onClicked: panelStack.currentIndex = 0
                }
                PreferenceCategoryButton {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    tabMode: true
                    text: qsTr("插件")
                    active: panelStack.currentIndex === 1
                    onClicked: panelStack.currentIndex = 1
                }
            }

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

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // ---- 宽面板：左侧纵向分类栏 ----
            Rectangle {
                visible: !root.compact
                Layout.preferredWidth: visible ? 104 : 0
                Layout.fillHeight: true
                color: "transparent"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingSm
                    spacing: Theme.spacingXs

                    PreferenceCategoryButton {
                        Layout.fillWidth: true
                        text: qsTr("主题")
                        active: panelStack.currentIndex === 0
                        onClicked: panelStack.currentIndex = 0
                    }
                    PreferenceCategoryButton {
                        Layout.fillWidth: true
                        text: qsTr("插件")
                        active: panelStack.currentIndex === 1
                        onClicked: panelStack.currentIndex = 1
                    }
                    Item {
                        Layout.fillHeight: true
                    }
                }

                Rectangle {
                    anchors {
                        right: parent.right
                        top: parent.top
                        bottom: parent.bottom
                    }
                    width: 1
                    color: Theme.border
                }
            }

            StackLayout {
                id: panelStack
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: 0

                // ==================== 主题页 ====================
                ScrollView {
                    id: themeScroll
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    ColumnLayout {
                        id: themeColumn
                        width: Math.max(0, Math.min(themeScroll.availableWidth - Theme.spacingLg * 2, 520))
                        x: Math.max(0, (themeScroll.availableWidth - width) / 2)
                        y: Theme.spacingLg
                        spacing: Theme.spacingLg

                        // 明暗模式：即时反映在下方标本上
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingMd

                            Label {
                                text: qsTr("主题")
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSizeCaption
                                Layout.alignment: Qt.AlignVCenter
                            }

                            ThemeModeSwitch {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 108
                                Layout.maximumWidth: 190
                                Layout.alignment: Qt.AlignVCenter
                                currentIndex: Theme.mode
                                onActivated: function (index) {
                                    Theme.setMode(index);
                                }
                            }

                            Item {
                                Layout.fillWidth: true
                            }

                            Button {
                                id: resetThemeButton
                                implicitWidth: 26
                                implicitHeight: 26
                                padding: 0
                                hoverEnabled: true
                                Layout.alignment: Qt.AlignVCenter
                                enabled: Theme.mode !== Theme.Mode.Light

                                Accessible.name: qsTr("恢复默认主题")
                                ToolTip.visible: hovered
                                ToolTip.delay: 500
                                ToolTip.text: qsTr("恢复默认主题，保留当前壁纸")

                                onClicked: Theme.setMode(Theme.Mode.Light)

                                background: Rectangle {
                                    radius: Theme.radiusControl
                                    color: resetThemeButton.hovered ? Theme.hoverOverlay : "transparent"
                                    border.width: resetThemeButton.visualFocus ? 1 : 0
                                    border.color: Theme.primary
                                }

                                contentItem: Item {
                                    Image {
                                        anchors.centerIn: parent
                                        width: 14
                                        height: 14
                                        source: "qrc:/images/toolbar/Common/restore-defaults.svg"
                                        sourceSize.width: 14
                                        sourceSize.height: 14
                                        fillMode: Image.PreserveAspectFit
                                        opacity: resetThemeButton.enabled ? 0.8 : 0.4
                                    }
                                }
                            }
                        }

                        // 壁纸：动作在标题行，预览即工作区标本
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingSm

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingSm

                                Label {
                                    text: qsTr("壁纸")
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontSizeCaption
                                }

                                Item {
                                    Layout.fillWidth: true
                                }

                                Button {
                                    id: wallpaperActionButton
                                    implicitHeight: 24
                                    padding: 4
                                    hoverEnabled: true
                                    Layout.alignment: Qt.AlignVCenter
                                    text: Theme.wallpaper !== "" ? qsTr("更换图片") : qsTr("添加图片")

                                    Accessible.name: text

                                    onClicked: wallpaperFileDialog.open()

                                    background: Rectangle {
                                        radius: Theme.radiusControl
                                        color: wallpaperActionButton.hovered ? Theme.hoverOverlay : "transparent"
                                        border.width: wallpaperActionButton.visualFocus ? 1 : 0
                                        border.color: Theme.primary
                                    }

                                    contentItem: Text {
                                        text: wallpaperActionButton.text
                                        color: wallpaperActionButton.hovered ? Theme.textPrimary : Theme.textSecondary
                                        font.pixelSize: Theme.fontSizeCaption
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }

                                Button {
                                    id: clearWallpaperButton
                                    visible: Theme.wallpaper !== ""
                                    implicitHeight: 24
                                    padding: 4
                                    hoverEnabled: true
                                    Layout.alignment: Qt.AlignVCenter
                                    text: qsTr("移除")

                                    Accessible.name: qsTr("移除背景壁纸")

                                    onClicked: {
                                        Wallpaper.removeWallpaper();
                                        Theme.setWallpaper("");
                                    }

                                    background: Rectangle {
                                        radius: Theme.radiusControl
                                        color: clearWallpaperButton.hovered ? Theme.hoverOverlay : "transparent"
                                        border.width: clearWallpaperButton.visualFocus ? 1 : 0
                                        border.color: Theme.primary
                                    }

                                    contentItem: Text {
                                        text: clearWallpaperButton.text
                                        color: clearWallpaperButton.hovered ? Theme.textPrimary : Theme.textSecondary
                                        font.pixelSize: Theme.fontSizeCaption
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                            }

                            Item {
                                Layout.fillWidth: true
                                Layout.preferredHeight: appearancePreview.implicitHeight

                                AppearancePreview {
                                    id: appearancePreview
                                    anchors.fill: parent
                                    wallpaperSource: Theme.wallpaperSource
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    radius: Theme.radiusMenu
                                    color: "transparent"
                                    border.width: previewHover.containsMouse ? 1 : 0
                                    border.color: Theme.primary
                                }

                                MouseArea {
                                    id: previewHover
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor

                                    Accessible.ignored: true

                                    onClicked: wallpaperFileDialog.open()
                                }
                            }
                        }
                    }
                }

                // ==================== 插件页 ====================
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: pluginListView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.margins: Theme.spacingMd
                        clip: true
                        spacing: 2
                        model: QModelManager.systemPluginManager.pluginNames
                        currentIndex: -1

                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 34
                            radius: Theme.radiusControl
                            color: pluginListView.currentIndex === index ? Theme.primaryTint : rowHover.containsMouse ? Theme.hoverOverlay : "transparent"
                            border.width: pluginListView.currentIndex === index ? 1 : 0
                            border.color: Theme.primary

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                anchors.right: parent.right
                                anchors.rightMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData
                                elide: Text.ElideRight
                                color: pluginListView.currentIndex === index ? Theme.primary : Theme.textPrimary
                                font.pixelSize: Theme.fontSizeBody
                                font.weight: pluginListView.currentIndex === index ? Font.DemiBold : Font.Normal
                            }

                            MouseArea {
                                id: rowHover
                                anchors.fill: parent
                                hoverEnabled: true
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

        // ---- 插件页底栏 ----
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            visible: root.pluginFooterVisible
            color: Theme.border
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.spacingSm
            spacing: Theme.spacingSm
            visible: root.pluginFooterVisible

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

    ThemedDialog {
        id: registerFailureDialog
        title: "注册失败"
        standardButtons: Dialog.Ok
        modal: true
        anchors.centerIn: parent

        Label {
            text: "插件注册失败"
        }
    }

    // 壁纸导入：选择本地图片，交由 C++ 校验并复制到托管目录
    FileDialog {
        id: wallpaperFileDialog
        title: qsTr("请选择背景图片")
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

    ThemedDialog {
        id: wallpaperFailureDialog
        title: qsTr("导入失败")
        standardButtons: Dialog.Ok
        modal: true
        anchors.centerIn: parent

        Label {
            text: qsTr("无法导入所选图片，请确认是有效的图片文件")
        }
    }
}
