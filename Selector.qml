/**
 * @file QSelector.qml
 * @brief 选择器，位于模型界面左上角，选择项类型与清空选择的交互界面
 */

import QtQuick 2.15
import QtQuick.Controls

Row{
    id:root

    signal selectorButtonClicked(int type)
    signal comboBoxSelectionChanged
    property QSelection selection
    property bool enabled: true
    
    ComboBox{
        id: selectModeComboBox
        enabled: root.enabled
        model:ListModel{
            ListElement{text: "边"}
            ListElement{text: "面"}
            ListElement{text: "块"}
        }
        onCurrentTextChanged: comboBoxSelectionChanged()
        opacity: root.enabled ? 1.0 : 0.5
    }
    Button{
        id: selectClearButton
        enabled: root.enabled
        text: "清除选择"
        onClicked:{
            root.selectorButtonClicked(0)
        }
        opacity: root.enabled ? 1.0 : 0.5
    }
    Button{
        enabled: root.enabled
        text: "确认"
        onClicked:{
            root.selectorButtonClicked(1)
        }
        opacity: root.enabled ? 1.0 : 0.5
    }
    /** type:string 选择框中当前文本 */
    property alias comboBoxSelectedString: selectModeComboBox.currentText
}
