import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: outputLog

    function appendMessage(message) {
        const viewport = logScroll.contentItem;
        const followEnd = viewport.atYEnd;
        const previousY = viewport.contentY;
        outputText.append(message);
        // append 会尝试显示旧光标，日志应以用户的滚动位置决定是否跟随。
        viewport.contentY = followEnd ? viewport.originY + Math.max(0, viewport.contentHeight - viewport.height) : previousY;
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 0

        ScrollView {
            id: logScroll
            objectName: "outputLogScroll"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ScrollBar.vertical: ScrollBar {
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                policy: ScrollBar.AsNeeded
                topPadding: 0
                bottomPadding: 0
            }

            TextArea {
                id: outputText
                objectName: "outputLogText"
                readOnly: true
                textFormat: TextEdit.RichText
                color: Theme.textPrimary
                font.family: Theme.monoFamily
                font.pixelSize: Theme.fontSizeCaption
                wrapMode: TextArea.Wrap
                text: ""

                Component.onCompleted: {
                    var msgs = QLogManager.messages;
                    for (var i = 0; i < msgs.length; i++) {
                        outputLog.appendMessage(msgs[i]);
                    }
                }
            }
        }
    }

    Connections {
        target: QLogManager
        function onNewMessage(level, message) {
            outputLog.appendMessage(message);
        }
    }
}
