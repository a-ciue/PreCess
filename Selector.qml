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
    ComboBox{
        id: selectModeComboBox
        model:ListModel{
            ListElement{text: "边"}
            ListElement{text: "面"}
            ListElement{text: "块"}
            ListElement{text: "组"}
        }
        onCurrentTextChanged: comboBoxSelectionChanged()
    }
    Button{
        id: selectClearButton
        text: "清除选择"
        onClicked:{
            root.selectorButtonClicked(0)
        }
    }
    Button{
        text: "确认"
        onClicked:{
            root.selectorButtonClicked(1)
        }
    }
    /** type:string 选择框中当前文本 */
    property alias comboBoxSelectedString: selectModeComboBox.currentText
}
