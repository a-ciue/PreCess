/**
 * @file DockWindow.qml
 * @brief 浮动窗口外壳：自绘边框 + 边缘缩放命中区（内容区由 C++ 注入）
 */

import QtQuick
import PreCess.Docking as Docking

Rectangle {
    id: root

    // 浮动窗口底与边框：取自 Docking.Theme.colors（与 app/Theme.qml 同步）
    readonly property color surfaceWindow: Docking.Theme.colors.windowBackground
    readonly property color windowBorder: Docking.Theme.colors.borderStrong

    color: surfaceWindow
    border.color: windowBorder
    border.width: 1

    // 自定义壁纸：由 app 侧 Binding 推入 Docking.Theme.wallpaperUrl；无壁纸时露出纯色底
    Image {
        anchors.fill: parent
        anchors.margins: 1
        source: Docking.Theme.wallpaperUrl
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        visible: source != ""
    }

    // 壁纸遮罩：随主题调色（浅色白亮化、深色灰暗化），无壁纸时不叠加
    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        color: Docking.Theme.colors.wallpaperTint
        visible: Docking.Theme.wallpaperUrl !== ""

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
