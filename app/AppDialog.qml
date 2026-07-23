/**
 * @file AppDialog.qml
 * @brief 可复用的信息展示弹窗：标题 + 可选择复制的文本内容 + 关闭按钮
 * @author 范成通 1941804585@qq.com
 * 全局经 App.showDialog(title, text) 调用（单实例注册在 App.registry.appDialog）。
 * 非模态，不阻断视图操作；标题栏可拖动；同一实例重复调用仅刷新内容，不叠加弹窗。
 */

import QtQuick
import QtQuick.Controls

Dialog {
    id: root
    modal: false
    standardButtons: Dialog.Close
    implicitWidth: 360
    implicitHeight: 240

    property alias text: contentText.text

    //! @brief 设置标题与内容并打开（重复调用仅刷新内容）
    function openWith(title, text) {
        root.title = title
        contentText.text = text
        root.open()
    }

    // 自定义标题栏：显示标题，并支持按住拖动弹窗（限制在窗口范围内）
    header: Rectangle {
        height: 32
        color: "#f0f0f0"

        Label {
            text: root.title
            font.bold: true
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
        }

        MouseArea {
            anchors.fill: parent
            property point pressPos
            onPressed: pressPos = Qt.point(mouseX, mouseY)
            onPositionChanged: {
                if (!pressed)
                    return
                const nx = root.x + mouseX - pressPos.x
                const ny = root.y + mouseY - pressPos.y
                root.x = Math.max(0, Math.min(nx, root.parent.width - root.width))
                root.y = Math.max(0, Math.min(ny, root.parent.height - root.height))
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        clip: true

        TextArea {
            id: contentText
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.Wrap
        }
    }
}
