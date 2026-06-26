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
    signal onSelectionConfirmed(QSelection selection)
    signal comboBoxSelectionChanged
    property QSelection selection
    property int cur_model

    SignalListener {
        id: confirm_listener
        target_signal: root.onSelectionConfirmed
    }
    
    Connections {
        target: root
        onSelectionChanged: {
            root.onSelectionConfirmed(root.selection)
        }
    }

    ComboBox{
        id: selectModeComboBox
        model:ListModel{
            ListElement{text: "..."}
            ListElement{text: "点"}
            ListElement{text: "边"}
            ListElement{text: "面"}
            ListElement{text: "体"}
            ListElement{text: "块"}
            ListElement{text: "几何点"}
            ListElement{text: "几何边"}
            ListElement{text: "几何面"}
            ListElement{text: "几何体"}
        }
        onCurrentTextChanged: comboBoxSelectionChanged()
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
        onClicked: root.confirmButtonClicked()
        enabled: root.cur_model >= 0 && confirm_listener.getListenerCount() > 0
        opacity: enabled ? 1.0 : 0.6
    }
    /** type:string 选择框中当前文本 */
    property alias comboBoxSelectedString: selectModeComboBox.currentText
    property alias confirm_listener: confirm_listener
}
