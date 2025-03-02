import QtQuick 2.15
import QtQuick.Controls

Row{
    id:root
    function changePropertyEnabled(){
        selectModeComboBox.enabled = !selectModeComboBox.enabled
    }
    signal selectorButtonClicked(int type)
    signal comboBoxSelectionChanged
    property QSelection selection
    ComboBox{
        id: selectModeComboBox
        // Layout.preferredWidth: 30
        //enabled: true
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
    property alias comboBoxSelectedString: selectModeComboBox.currentText
}
