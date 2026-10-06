/**
 * @file DockWindow.qml
 * @brief 浮动窗口外壳：自绘边框 + 边缘缩放命中区（内容区由 C++ 注入）
 */

import QtQuick

Rectangle {
    id: root

    // 浮动窗口底与边框（与 app 侧 Theme 令牌取值保持一致；dock 模块
    // 刻意不依赖 app，故此处以局部常量对齐配色）
    // 修改这些配色时须同步检查 app/Theme.qml 中的对应令牌。
    readonly property color surfaceWindow: "#F3F4F6"
    readonly property color windowBorder: "#C3C8CF"

    color: surfaceWindow
    border.color: windowBorder
    border.width: 1

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
