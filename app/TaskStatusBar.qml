/**
 * @file TaskStatusBar.qml
 * @brief 状态文字、任务进度和取消入口，统一绑定共享 QTaskStatus。
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import app.model

Rectangle {
    id: root
    height: 26
    color: "#f0f0f0"

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: "#c8c8c8"
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 8

        Label {
            id: statusText
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            font.pixelSize: 12
            color: "#444441"
            elide: Text.ElideRight
            text: {
                var task = QModelManager.taskStatus;
                if (task.message !== "")
                    return task.message.replace(/\s*\n\s*/g, " ");
                return task.running ? qsTr("执行中…") : qsTr("就绪");
            }
            ToolTip.visible: statusHover.hovered && truncated
            ToolTip.delay: 400
            ToolTip.text: QModelManager.taskStatus.message
            HoverHandler {
                id: statusHover
            }
        }

        ProgressBar {
            Layout.preferredWidth: 220
            Layout.minimumWidth: 80
            visible: QModelManager.taskStatus.running
            from: 0
            to: 1
            indeterminate: QModelManager.taskStatus.progress <= 0
            value: QModelManager.taskStatus.progress
        }

        Button {
            id: cancelButton
            visible: QModelManager.taskStatus.running
            padding: 3
            hoverEnabled: true
            background: Rectangle {
                radius: 3
                color: cancelButton.hovered ? "#fbe4e4" : "transparent"
            }
            contentItem: Text {
                text: "✕"
                color: "#c62828"
                font.pixelSize: 12
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            ToolTip.visible: hovered
            ToolTip.delay: 400
            ToolTip.text: qsTr("取消执行")
            onClicked: QModelManager.taskStatus.cancel()
        }

        Label {
            visible: QModelManager.taskStatus.running && QModelManager.taskStatus.progress > 0
            font.pixelSize: 12
            color: "#444441"
            text: Math.round(QModelManager.taskStatus.progress * 100) + "%"
        }

        Label {
            visible: QModelManager.taskStatus.writePending
            font.pixelSize: 11
            color: "#ef6c00"
            text: qsTr("任务处理中…（模型编辑暂不可用）")
        }
    }
}
