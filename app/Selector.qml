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

    // 当前角度扩散参数，由渲染区域传入。
    property bool faceSelectByAngle
    property real faceSelectAngle

    // 将用户编辑后的参数通知给渲染区域。
    signal faceSelectionSpreadEdited(bool enabled, real angle)

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
            if(currentText === "几何点"){
                App.selection.selectMode = "GeometryVertex"
            }
            if(currentText === "几何边"){
                App.selection.selectMode = "GeometryEdge"
            }
            if(currentText === "几何面"){
                App.selection.selectMode = "GeometryFace"
            }
            if(currentText === "几何体"){
                App.selection.selectMode = "GeometrySolid"
            }
            if(currentText === "组件"){
                App.selection.selectMode = "Component"
            }
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
    CheckBox {
        text: "按角度扩散"
        checked: root.faceSelectByAngle
        visible: App.selection.selectMode === "Face"
        onClicked: root.faceSelectionSpreadEdited(checked, root.faceSelectAngle)
    }
    Label {
        text: "角度"
        visible: App.selection.selectMode === "Face" && root.faceSelectByAngle
    }
    TextField {
        text: root.faceSelectAngle.toFixed(1)
        visible: App.selection.selectMode === "Face" && root.faceSelectByAngle
        Layout.preferredWidth: 56
        validator: DoubleValidator {
            bottom: 0.0
            top: 180.0
            decimals: 2
        }
        onEditingFinished: {
            var value = Number(text)
            if (!isNaN(value))
                root.faceSelectionSpreadEdited(root.faceSelectByAngle,Math.max(0.0, Math.min(180.0, value)))
        }
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
