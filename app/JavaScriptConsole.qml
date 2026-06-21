import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import app.core
import app.model

Item {
    id: jsConsole

    // 关闭请求信号
    signal closeRequested()

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
                          "按 F10 切换控制台显示\n" +
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

                    // 处理输入文本变化，检测是否输入了父组件ID并按了.
                    onTextChanged: {
                        // 检查文本是否包含点号，表示正在访问子组件
                        if (text.endsWith(".")) {
                            let parentId = text.substring(0, text.length - 1).trim()
                            if (parentId) {
                                // 尝试获取父组件
                                let parentComponent = findComponentById(parentId)
                                if (parentId) {
                                    // 获取子组件列表并显示
                                    var childrenList = getChildrenList(parentComponent)
                                    outputText.text += "子组件列表 (" + parentId + "):\n"
                                    outputText.text += childrenList + "\n"
                                    outputText.cursorPosition = outputText.length
                                }
                            }
                        }
                    }

                    // 处理Tab键，显示子组件
                    Keys.onPressed: {
                        if (event.key === Qt.Key_Tab) {
                            event.accepted = true
                            // 如果文本以点结尾，显示子组件
                            if (text.endsWith(".")) {
                                let parentId = text.substring(0, text.length - 1).trim()
                                if (parentId) {
                                    try {
                                        let parentComponent = findComponentById(parentId)
                                        if (parentId) {
                                            let childrenList = getChildrenList(parentComponent)
                                            outputText.text += "子组件列表 (" + parentId + "):\n"
                                            outputText.text += childrenList + "\n"
                                            outputText.cursorPosition = outputText.length
                                        } else {
                                            outputText.text += "✗ 错误: 未找到组件 " + parentId + "\n"
                                        }
                                    } catch (e) {
                                        outputText.text += "✗ 错误: " + e.toString() + "\n"
                                    }
                                }
                            }
                        }
                    }

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

    // 执行命令的函数
    function executeCommand(cmd) {
        outputText.text += "> " + cmd + "\n"

        // 特殊命令处理
        if (cmd.trim() === "help") {
            showQuickHelp()
            return
        } else if (cmd.trim() === "clear") {
            outputText.text = "=== PreCess JavaScript 控制台 ===\n\n"
            return
        } else if (cmd.trim() === "components") {
            outputText.text += "← 全局对象: App (App.selection, App.registry, App.activeOperation)\n"
            outputText.text += "← 系统对象: QModelManager (observer, query, algorithmSystem, ioSystem, editSystem)\n"
            outputText.text += "← 注册控件: App.registry.renderWindow, App.registry.treeModel\n\n"
            return
        }

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

    // 获取子组件列表的函数
    function getChildrenList(parent) {
        let result = ""
        
        // 对于Item类型的组件，使用children属性
        if (parent.children && parent.children.length > 0) {
            result += "  item类型组件:\n"
            for (let i = 0; i < parent.children.length; i++) {
                let child = parent.children[i]
                let childType = child.toString().split(" ")[0] || "Unknown"
                
                result += "  - " + " (" + childType + ")\n"
                
                // 递归获取更深层的子组件（可选，这里只显示一层）
            }
        } else {
            result += "  (无子组件)\n"
        }
        
        // 对于非Item组件，检查contentChildren属性
        if (parent.contentChildren && parent.contentChildren.length > 0) {
            result += "  控件类型组件:\n"
            for (let j = 0; j < parent.contentChildren.length; j++) {
                let contentChild = parent.contentChildren[j]
                let contentChildType = contentChild.toString().split(" ")[0] || "Unknown"
                
                result += "    - " + " (" + contentChildType + ")\n"
            }
        }
        
        // 检查data属性（适用于Item的data属性）
        if (parent.data && parent.data.length > 0) {
            result += "  数据类型组件:\n"
            for (let k = 0; k < parent.data.length; k++) {
                let dataChild = parent.data[k]
                if (dataChild) {
                    let dataChildType = dataChild.toString().split(" ")[0] || "Unknown"
                    
                    result += "    - " + " (" + dataChildType + ")\n"
                }
            }
        }
        
        return result
    }

    // 显示快速帮助信息
    function showQuickHelp() {
        let helpText = "🚀 快速命令:\n" +
                       "• help - 显示此帮助\n" +
                       "• clear - 清空控制台\n" +
                       "• components - 显示可用组件\n\n" +
                       "🔧 组件探索:\n" +
                       "• 通过 App.registry.xxx 访问注册控件\n" +
                       "• 例如: App.registry.renderWindow.resetCamera()\n" +
                       "• Tab 键查看对象子属性\n\n" +
                       "💡 提示: 直接输入表达式执行 JavaScript\n"

        outputText.text += helpText + "\n"
        outputText.cursorPosition = outputText.length
    }

    // 专门用于查找组件的包装函数
    function findComponentById(id) {
        // 使用安全的方式查找组件
        try {
            let component = eval(id)
            return component
        } catch (e) {
            console.log("查找组件失败: " + e.toString())
            return null
        }
    }
}