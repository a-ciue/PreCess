/**
 * @file QSelector.qml
 * @brief 选择器，位于模型界面左上角，选择项类型与清空选择的交互界面
 */

import QtQuick 2.15
import QtQuick.Layouts
import QtQuick.Controls
import app.core

RowLayout {
    id:root

    signal clearButtonClicked
    signal confirmButtonClicked
    signal comboBoxSelectionChanged
    property QSelection selection
    property bool enabled: false
    
    ComboBox{
        id: selectModeComboBox
        model:ListModel{
            ListElement{text: "..."}
            ListElement{text: "点"}
            ListElement{text: "边"}
            ListElement{text: "面"}
            ListElement{text: "体"}
            ListElement{text: "块"}
        }
        onCurrentTextChanged: comboBoxSelectionChanged()
        opacity: root.enabled ? 1.0 : 0.5
    }
    Button{
        id: selectClearButton
        text: "清除选择"
        onClicked:{
            root.clearButtonClicked()
        }
        opacity: root.enabled ? 1.0 : 0.5
    }
    Button{
        visible: root.enabled
        text: "确认"
        onClicked:{
            root.confirmButtonClicked()
        }
        opacity: root.enabled ? 1.0 : 0.5
    }
    /** type:string 选择框中当前文本 */
    property alias comboBoxSelectedString: selectModeComboBox.currentText
}
