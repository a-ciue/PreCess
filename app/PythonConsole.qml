/**
 * @file PythonConsole.qml
 * @brief Python 控制台：经 QModelManager.pythonRuntime 驱动内嵌解释器
 *
 * 交互模型：输入行是纯 Python，无界面级命令；help/clear/exit 由运行时以
 * Python 函数注入（QPythonRuntime::ensureInitialized）。执行约定：
 * Python ≡ GUI 线程，execute 同步返回 {ok, incomplete, output, error}。
 * 输入未完（incomplete，如 def/for/if 块）时提示符切为 "..." 并续行拼接，
 * Esc 放弃续行；上下箭头翻历史；clear() 经输出换页符 \f 由本组件识别清屏；
 * 帮助按钮等价于执行 help()。
 * @sa JavaScriptConsole.qml
 * @sa QPythonRuntime
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import app.model

Item {
    id: pyConsole

    // 未执行完的多行输入缓冲（续行期间逐行拼接，执行成功或放弃后清空）
    property string buffer: ""
    property var history: []
    property int historyIndex: -1
    property bool booted: false

    function prompt() {
        return buffer.length > 0 ? "..." : ">"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 6

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
            }

            TextArea {
                id: outputText
                readOnly: true
                color: "#333333"
                font.family: "Courier New"
                font.pixelSize: 12
                wrapMode: TextArea.Wrap
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                id: promptText
                text: ">"
                color: "#1976d2"
                font.pixelSize: 14
                font.bold: true
            }

            TextField {
                id: inputField
                Layout.fillWidth: true
                placeholderText: "输入 Python 代码..."
                color: "#333333"
                font.family: "Courier New"
                font.pixelSize: 12

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
                    if (text.trim() !== "" || pyConsole.buffer.length > 0)
                        submitInput()
                }

                Keys.onEscapePressed: {
                    if (pyConsole.buffer.length > 0) {
                        pyConsole.buffer = ""
                        promptText.text = ">"
                        appendOutput("（已放弃续行）\n\n")
                    }
                }
            }

            Button {
                text: "帮助"
                onClicked: {
                    // 与输入 help() 等价：以命令形式执行，展示交互方式
                    inputField.text = "help()"
                    submitInput()
                    inputField.forceActiveFocus()
                }
            }

            Button {
                text: "执行"
                highlighted: true
                onClicked: {
                    if (inputField.text.trim() !== "" || pyConsole.buffer.length > 0) {
                        submitInput()
                        inputField.forceActiveFocus()
                    }
                }
            }

            Button {
                text: "清空窗口"
                onClicked: {
                    outputText.text = ""
                    pyConsole.buffer = ""
                    promptText.text = ">"
                }
            }
        }
    }

    Component.onCompleted: {
        if (visible)
            boot()
    }

    onVisibleChanged: {
        if (visible && !booted)
            boot()
    }

    // 首次显示时初始化运行环境并输出欢迎信息（解释器懒启动，启动耗时只发生一次）
    function boot() {
        booted = true
        const runtime = QModelManager.pythonRuntime
        runtime.initialize()
        appendOutput("=== PreCess Python 控制台 ===\n")
        appendOutput("按 F11 切换控制台显示\n")
        if (runtime.available) {
            appendOutput("解释器: " + runtime.version() + "\n")
            appendOutput("----------------------------\n")
            appendOutput("import precess 后经 precess.current 操作当前会话\n")
            appendOutput("输入 help() 查看控制台指南（help(对象) 查看文档）\n\n")
        } else {
            appendOutput("----------------------------\n")
            appendOutput("✗ " + runtime.lastError() + "\n\n")
        }
        inputField.forceActiveFocus()
    }

    function appendOutput(text) {
        outputText.text += text
        outputText.cursorPosition = outputText.length
    }

    function submitInput() {
        const line = inputField.text
        appendOutput(prompt() + " " + line + "\n")
        inputField.text = ""
        history.push(line)
        historyIndex = history.length

        // 续行期间整块重交编译：未完不执行（compile_command 纯编译），完成才运行
        pyConsole.buffer += line + "\n"
        const result = QModelManager.pythonRuntime.execute(pyConsole.buffer)
        if (result.incomplete) {
            promptText.text = pyConsole.prompt()
            outputText.cursorPosition = outputText.length
            return
        }

        pyConsole.buffer = ""
        promptText.text = ">"
        // 输入行是纯 Python，没有界面级命令：clear() 以输出换页符 \f 约定清屏
        if (result.output.indexOf("\f") >= 0) {
            outputText.text = ""
            const rest = result.output.substring(result.output.lastIndexOf("\f") + 1)
            if (rest.length > 0)
                outputText.text = rest
        } else if (result.output.length > 0) {
            appendOutput(result.output)
        }
        if (!result.ok && result.error.length > 0)
            appendOutput("✗ " + result.error.trim() + "\n")
        appendOutput("\n")
    }
}
