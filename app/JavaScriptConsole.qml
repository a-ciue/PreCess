import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: jsConsole
    width: consoleDrawer.width
    height: consoleDrawer.height
    
    // 外部控制的可见性属性
    property bool consoleVisible: false
    
    // 关闭请求信号
    signal closeRequested()
    
    // 控制台抽屉
    Drawer {
        id: consoleDrawer
        width: parent.width
        height: parent.height * 0.3
        edge: Qt.BottomEdge
        visible: jsConsole.consoleVisible  // 直接绑定到外部属性

        Rectangle {
            anchors.fill: parent
            color: "#1e1e1e"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                // 标题栏
                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: "JavaScript 控制台"
                        color: "#ffffff"
                        font.pixelSize: 16
                        font.bold: true
                        Layout.fillWidth: true
                    }

                    Button {
                        text: "清空"
                        onClicked: outputText.text = ""
                    }

                    Button {
                        text: "关闭"
                        onClicked: {
                            // 发送关闭请求，而不是直接修改状态
                            jsConsole.closeRequested()
                        }        
                    }
                }

                // 输出区域
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    TextArea {
                        id: outputText
                        readOnly: true
                        color: "#ffffff"
                        font.family: "Courier New"
                        font.pixelSize: 12
                        wrapMode: TextArea.Wrap
                        background: Rectangle {
                            color: "#2d2d2d"
                            border.color: "#3d3d3d"
                        }
                        text: "=== PreCess JavaScript 控制台 ===\n" +
                              "按 Tab 切换控制台显示\n" +
                              "----------------------------\n\n"
                    }
                }

                // 输入区域
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Text {
                        text: ">"
                        color: "#4ec9b0"
                        font.pixelSize: 14
                        font.bold: true
                    }

                    TextField {
                        id: inputField
                        Layout.fillWidth: true
                        placeholderText: "输入 JavaScript 代码..."
                        color: "#ffffff"
                        font.family: "Courier New"
                        font.pixelSize: 12

                        background: Rectangle {
                            color: "#2d2d2d"
                            border.color: inputField.activeFocus ? "#007acc" : "#3d3d3d"
                            border.width: 1
                        }

                        property var history: []
                        property int historyIndex: -1

                        Keys.onUpPressed: {
                            if (history.length > 0) {
                                historyIndex = Math.max(0, historyIndex - 1)
                                text = history[historyIndex]
                            }
                        }

                        Keys.onDownPressed: {
                            if (history.length > 0) {
                                historyIndex = Math.min(history.length - 1, historyIndex + 1)
                                text = history[historyIndex]
                            }
                        }

                        Keys.onReturnPressed: {
                            if (text.trim() !== "") {
                                executeCommand(text)
                                history.push(text)
                                historyIndex = history.length
                                text = ""
                            }
                        }
                    }

                    Button {
                        text: "执行"
                        highlighted: true
                        onClicked: {
                            if (inputField.text.trim() !== "") {
                                executeCommand(inputField.text)
                                inputField.history.push(inputField.text)
                                inputField.historyIndex = inputField.history.length
                                inputField.text = ""
                            }
                        }
                    }
                }
            }
        }
    }

    // 执行命令的函数
    function executeCommand(cmd) {
        outputText.text += "> " + cmd + "\n"

        try {
            var result = eval(cmd)

            if (result !== undefined) {
                var resultStr = ""
                if (typeof result === "object" && result !== null) {
                    try {
                        resultStr = JSON.stringify(result, null, 2)
                    } catch (e) {
                        resultStr = result.toString()
                    }
                } else {
                    resultStr = String(result)
                }
                outputText.text += "← " + resultStr + "\n"
            } else {
                outputText.text += "← undefined\n"
            }
        } catch (e) {
            outputText.text += "✗ 错误: " + e.toString() + "\n"
        }

        outputText.text += "\n"
        outputText.cursorPosition = outputText.length
    }

    function findChild(parent, objectName) {
        if (!parent) return null
        for (var i = 0; i < parent.children.length; i++) {
            var child = parent.children[i]
            if (child.objectName === objectName) {
                return child
            }
            var found = findChild(child, objectName)
            if (found) return found
        }
        return null
    }

    function listChildren(parent, indent) {
        if (!parent) return ""
        indent = indent || ""
        var result = ""
        for (var i = 0; i < parent.children.length; i++) {
            var child = parent.children[i]
            var name = child.objectName || child.toString()
            result += indent + "- " + name + "\n"
            result += listChildren(child, indent + "  ")
        }
        return result
    }
}
