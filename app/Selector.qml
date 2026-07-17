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
        model: ListModel {
            ListElement { text: "..." }
            ListElement { text: "点" }
            ListElement { text: "边" }
            ListElement { text: "面" }
            ListElement { text: "体" }
            ListElement { text: "块" }
            ListElement{text: "几何点"}
            ListElement{text: "几何边"}
            ListElement{text: "几何面"}
            ListElement{text: "几何体"}
            ListElement{text: "组件"}
        }
        currentIndex: {
            switch (App.selection.selectMode) {
            case "None": return 0;
            case "Vertex": return 1;
            case "Edge": return 2;
            case "Face": return 3;
            case "Solid": return 4;
            case "Block": return 5;
            case "GeometryVertex": return 6;
            case "GeometryEdge": return 7;
            case "GeometryFace": return 8;
            case "GeometrySolid": return 9;
            case "Component": return 10;
            default: return 0;
            }
        }
        onActivated: (index) => {
            const text = selectModeComboBox.textAt(index)
            if (text === "...") {
                App.selection.selectMode = "None"
            } else if (text === "点") {
                App.selection.selectMode = "Vertex"
            } else if (text === "边") {
                App.selection.selectMode = "Edge"
            } else if (text === "面") {
                App.selection.selectMode = "Face"
            } else if (text === "体") {
                App.selection.selectMode = "Solid"
            } else if (text === "块") {
                App.selection.selectMode = "Block"
            } else if (text === "几何点") {
                App.selection.selectMode = "GeometryVertex"
            } else if (text === "几何边") {
                App.selection.selectMode = "GeometryEdge"
            } else if (text === "几何面") {
                App.selection.selectMode = "GeometryFace"
            } else if (text === "几何体") {
                App.selection.selectMode = "GeometrySolid"
            } else if (text === "组件") {
                App.selection.selectMode = "Component"
            }
        }
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
