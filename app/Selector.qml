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
    property int cur_model

    ComboBox{
        id: selectModeComboBox
        model: ListModel {
            ListElement { text: "..." }
            ListElement { text: "点" }
            ListElement { text: "边" }
            ListElement { text: "面" }
            ListElement { text: "体" }
            ListElement { text: "块" }
        }
        onCurrentTextChanged: {
            if(currentText === "..."){
                App.selection.selectMode = "None"
            }
            if(currentText === "点"){
                App.selection.selectMode = "Vertex"
            }
            if(currentText === "边"){
                App.selection.selectMode = "Edge"
            }
            if(currentText === "面"){
                App.selection.selectMode = "Face"
            }
            if(currentText === "块"){
                App.selection.selectMode = "Block"
            }
            if(currentText === "体"){
                App.selection.selectMode = "Solid"
            }
        }
        enabled: root.cur_model >= 0
        opacity: enabled ? 1.0 : 0.6
        currentIndex: enabled ? currentIndex: 0
    }
    Button{
        id: selectClearButton
        text: "清除选择"
        onClicked: root.clearButtonClicked()
        enabled: root.cur_model >= 0
        opacity: enabled ? 1.0 : 0.6
    }
    Button{
        text: "确认"
        onClicked: {
            root.confirmButtonClicked()
            App.selection.confirmed(root.selection)
        }
        enabled: root.cur_model >= 0 && App.selection.listeningSelectorIndex >= 0
        opacity: enabled ? 1.0 : 0.6
    }
    /** type:string 选择框中当前文本 */
    property alias comboBoxSelectedString: selectModeComboBox.currentText
}
