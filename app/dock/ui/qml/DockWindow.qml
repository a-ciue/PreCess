/**
 * @file DockWindow.qml
 * @brief 浮动窗口外壳：自绘边框 + 边缘缩放命中区（内容区由 C++ 注入）
 */

import QtQuick
import PreCess.Docking as Docking

Rectangle {
    id: root

    // 浮动窗口底与边框：取自 app 侧推送的 Docking.Theme.colors
    readonly property color surfaceWindow: Docking.Theme.colors.windowBackground
    readonly property color windowBorder: Docking.Theme.colors.borderStrong

    color: surfaceWindow
    border.color: windowBorder
    border.width: 1

    // 浮窗是独立 QQuickWindow，不会继承主窗口 palette；在内容公共祖先绑定
    // 普通控件调色板，使既有面板与之后创建的输入框/下拉框自然继承。
    palette.window: Docking.Theme.controlPalette.window
    palette.windowText: Docking.Theme.controlPalette.windowText
    palette.base: Docking.Theme.controlPalette.base
    palette.alternateBase: Docking.Theme.controlPalette.alternateBase
    palette.text: Docking.Theme.controlPalette.text
    palette.button: Docking.Theme.controlPalette.button
    palette.buttonText: Docking.Theme.controlPalette.buttonText
    palette.highlight: Docking.Theme.controlPalette.highlight
    palette.highlightedText: Docking.Theme.controlPalette.highlightedText
    palette.placeholderText: Docking.Theme.controlPalette.placeholderText
    palette.mid: Docking.Theme.controlPalette.mid
    palette.midlight: Docking.Theme.controlPalette.midlight
    palette.light: Docking.Theme.controlPalette.light
    palette.dark: Docking.Theme.controlPalette.dark
    palette.shadow: Docking.Theme.controlPalette.shadow
    palette.link: Docking.Theme.controlPalette.link
    palette.toolTipBase: Docking.Theme.controlPalette.toolTipBase
    palette.toolTipText: Docking.Theme.controlPalette.toolTipText

    // 自定义壁纸：由 app 侧 Binding 推入 Docking.Theme.wallpaperUrl；无壁纸时露出纯色底
    Image {
        id: wallpaperImage
        anchors.fill: parent
        anchors.margins: 1
        source: Docking.Theme.wallpaperUrl
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        visible: source.toString() !== "" && status !== Image.Error
    }

    // 壁纸遮罩：随主题调色（浅色白亮化、深色灰暗化），无壁纸或加载失败时不叠加
    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        color: Docking.Theme.colors.wallpaperTint
        visible: Docking.Theme.wallpaperUrl !== "" && wallpaperImage.status === Image.Ready

        Behavior on color {
            ColorAnimation {
                duration: 300
                easing.type: Easing.InOutCubic
            }
        }
    }

    // 停靠区域宿主：C++ 侧把 DockAreaItem 挂到此处
    Item {
        id: areaHost
        objectName: "areaHost"
        anchors.fill: parent
        anchors.margins: 1
        clip: true
    }

    component ResizeHandle: MouseArea {
        property int edges: 0
        acceptedButtons: Qt.LeftButton
        onPressed: {
            if (root.Window.window)
                root.Window.window.startSystemResize(edges)
        }
    }

    ResizeHandle {
        edges: Qt.LeftEdge
        cursorShape: Qt.SizeHorCursor
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
        width: 5
    }
    ResizeHandle {
        edges: Qt.RightEdge
        cursorShape: Qt.SizeHorCursor
        anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
        width: 5
    }
    ResizeHandle {
        edges: Qt.TopEdge
        cursorShape: Qt.SizeVerCursor
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 5
    }
    ResizeHandle {
        edges: Qt.BottomEdge
        cursorShape: Qt.SizeVerCursor
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 5
    }
    ResizeHandle {
        edges: Qt.LeftEdge | Qt.TopEdge
        cursorShape: Qt.SizeFDiagCursor
        anchors { left: parent.left; top: parent.top }
        width: 8
        height: 8
    }
    ResizeHandle {
        edges: Qt.RightEdge | Qt.BottomEdge
        cursorShape: Qt.SizeFDiagCursor
        anchors { right: parent.right; bottom: parent.bottom }
        width: 8
        height: 8
    }
    ResizeHandle {
        edges: Qt.RightEdge | Qt.TopEdge
        cursorShape: Qt.SizeBDiagCursor
        anchors { right: parent.right; top: parent.top }
        width: 8
        height: 8
    }
    ResizeHandle {
        edges: Qt.LeftEdge | Qt.BottomEdge
        cursorShape: Qt.SizeBDiagCursor
        anchors { left: parent.left; bottom: parent.bottom }
        width: 8
        height: 8
    }
}
