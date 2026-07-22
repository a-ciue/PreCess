/**
 * @file Selector.qml
 * @brief 选择器，位于模型界面左上角，选择项类型与清空选择的交互界面
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import app.core

RowLayout {
    id:root
    signal clearButtonClicked
    signal confirmButtonClicked
    property QSelection selection

    ComboBox{
        id: selectModeComboBox
        model: [
            { text: "...", value: "None" },
            { text: "点", value: "Vertex" },
            { text: "边", value: "Edge" },
            { text: "面", value: "Face" },
            { text: "体", value: "Solid" },
            { text: "几何点", value: "GeometryVertex" },
            { text: "几何边", value: "GeometryEdge" },
            { text: "几何面", value: "GeometryFace" },
            { text: "几何体", value: "GeometrySolid" },
            { text: "组件", value: "Component" }
        ]
        textRole: "text"
        valueRole: "value"

        // 绑定建立在自己的属性上——ComboBox 内部不会碰它，绑定不会被用户交互破坏
        property string sourceMode: App.selection.selectMode

        // 外部值变化 → 更新显示（初始化时也会触发一次，不用 Component.onCompleted）
        onSourceModeChanged: {
            const idx = indexOfValue(sourceMode)
            if (idx >= 0)
                currentIndex = idx
        }

        // 用户选择 → 写回数据源
        onActivated: App.selection.selectMode = currentValue

        opacity: enabled ? 1.0 : 0.6
    }
    Button{
        id: selectClearButton
        text: "清除选择"
        onClicked: root.clearButtonClicked()
        opacity: enabled ? 1.0 : 0.6
    }
    Button{
        text: "确认"
        onClicked: {
            root.confirmButtonClicked()
            App.selection.confirmed(root.selection)
        }
        enabled: App.selection.listeningSelectorIndex >= 0
        opacity: enabled ? 1.0 : 0.6
    }
    /** type:string 选择框中当前文本 */
    property alias comboBoxSelectedString: selectModeComboBox.currentText
}
