import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: outputLog

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 0

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ScrollBar.vertical: ScrollBar {
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                policy: ScrollBar.AsNeeded
                topPadding: 0
                bottomPadding: 0
                background: Rectangle {
                    implicitWidth: 8
                    color: "transparent"
                }
                contentItem: Rectangle {
                    implicitWidth: 8
                    radius: 4
                    color: parent.hovered ? "#c0c0c0" : "#e0e0e0"
                }
            }

            TextArea {
                id: outputText
                readOnly: true
                textFormat: TextEdit.RichText
                color: "#333333"
                font.family: "Courier New"
                font.pixelSize: 12
                wrapMode: TextArea.Wrap
                background: Rectangle {
                    color: "#f0f0f0"
                    border.color: "#d0d0d0"
                    border.width: 1
                }
                text: ""

                Component.onCompleted: {
                    var msgs = QLogManager.messages
                    for (var i = 0; i < msgs.length; i++) {
                        outputText.append(msgs[i])
                    }
                }
            }
        }
    }

    Connections {
        target: QLogManager
        function onNewMessage(level, message) {
            outputText.append(message)
        }
    }
}
