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

    ListModel {
        id: selectModeModel
        ListElement { text: "..."; mode: "None" }
        ListElement { text: "点"; mode: "Vertex" }
        ListElement { text: "边"; mode: "Edge" }
        ListElement { text: "面"; mode: "Face" }
        ListElement { text: "体"; mode: "Solid" }
        ListElement { text: "块"; mode: "Block" }
        ListElement { text: "几何点"; mode: "GeometryVertex" }
        ListElement { text: "几何边"; mode: "GeometryEdge" }
        ListElement { text: "几何面"; mode: "GeometryFace" }
        ListElement { text: "几何体"; mode: "GeometrySolid" }
        ListElement { text: "组件"; mode: "Component" }
    }

    // 选择模式与界面选项共用同一数据源，新增模式时只需增加一个 ListElement。
    function indexForMode(mode) {
        for (let i = 0; i < selectModeModel.count; ++i) {
            if (selectModeModel.get(i).mode === mode)
                return i
        }
        return -1
    }

    ComboBox{
        id: selectModeComboBox
        model: selectModeModel
        textRole: "text"
        onCurrentIndexChanged: {
            if (currentIndex >= 0)
                App.selection.selectMode = selectModeModel.get(currentIndex).mode
        }
        opacity: enabled ? 1.0 : 0.6
        currentIndex: enabled ? currentIndex: 0
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
    Connections {
        target: App.selection
        function onSelectModeChanged() {
            let modeIndex = root.indexForMode(App.selection.selectMode)
            if (modeIndex >= 0 && selectModeComboBox.currentIndex !== modeIndex)
                selectModeComboBox.currentIndex = modeIndex
        }
    }
    /** type:string 选择框中当前文本 */
    property alias comboBoxSelectedString: selectModeComboBox.currentText
}
