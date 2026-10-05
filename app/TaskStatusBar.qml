/**
 * @file TaskStatusBar.qml
 * @brief 底部状态栏控件：状态文本 + 进度条 + 取消红叉 + 百分比 + 回写待定徽标
 *
 * 数据源统一为 QModelManager.taskStatus（共享任务状态源），不点名具体系统——
 * 忙标志（writePending/frozenBusy）与进度均由 QTaskStatus 单点暴露。
 * statusMessage 由主窗口的终态反馈连接写入（任务失败/取消提示，成功复位）。
 */
import QtQuick
import QtQuick.Controls

import app.model

Rectangle {
    id: root
    height: 26
    color: "#f0f0f0"
    property string statusMessage: ""

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: "#c8c8c8"
    }

    Label {
        id: statusText
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        font.pixelSize: 12
        color: "#444441"
        text: {
            var task = QModelManager.taskStatus
            if (task.running)
                return task.progressLabel !== "" ? task.progressLabel : qsTr("执行中…")
            if (root.statusMessage !== "")
                return root.statusMessage
            return qsTr("就绪")
        }
    }

    ProgressBar {
        id: statusBarProgress
        anchors.left: statusText.right
        anchors.leftMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        width: 220
        visible: QModelManager.taskStatus.running
        from: 0
        to: 1
        // 任务尚未上报进度时显示跑马灯，有上报后切换为确定进度
        indeterminate: QModelManager.taskStatus.progress <= 0
        value: QModelManager.taskStatus.progress
    }

    // 取消：紧邻进度条的红叉（任何任务心跳点生效；无进度插桩的任务不可中断）
    Button {
        id: cancelButton
        anchors.left: statusBarProgress.right
        anchors.leftMargin: 6
        anchors.verticalCenter: parent.verticalCenter
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
        id: progressPct
        anchors.left: cancelButton.right
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        visible: QModelManager.taskStatus.running && QModelManager.taskStatus.progress > 0
        font.pixelSize: 12
        color: "#444441"
        text: Math.round(QModelManager.taskStatus.progress * 100) + "%"
    }

    // 软冻结显形：带回写任务"回写未落地期"写闸拒改（可看不可改）；
    // 占用期 undo/redo 拒绝；取消仅请求停止，GUI 收尾结束后才释放占用。
    Label {
        anchors.left: progressPct.right
        anchors.leftMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        visible: QModelManager.taskStatus.writePending
        font.pixelSize: 11
        color: "#ef6c00"
        text: qsTr("任务处理中…（模型编辑暂不可用）")
    }
}
