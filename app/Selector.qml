/**
 * @file Selector.qml
 * @brief 选择器，位于模型界面左上角，选择项类型与清空选择的交互界面
 */

import QtQuick
import QtQuick.Shapes
import QtQuick.Layouts
import QtQuick.Controls.Fusion
import app.core

RowLayout {
    id:root
    signal clearButtonClicked

    // 当前角度扩散参数，由渲染区域传入。
    property bool faceSelectByAngle
    property real faceSelectAngle

    // 将用户编辑后的参数通知给渲染区域。
    signal faceSelectionSpreadEdited(bool enabled, real angle)

    ComboBox{
        id: selectModeComboBox
        model: [
            { text: "...", value: "None" },
            { text: "组件", value: "Component" },
            { text: "点", value: "Vertex" },
            { text: "边", value: "Edge" },
            { text: "面", value: "Face" },
            { text: "体", value: "Solid" },
            { text: "几何点", value: "GeometryVertex" },
            { text: "几何边", value: "GeometryEdge" },
            { text: "几何面", value: "GeometryFace" },
            { text: "几何体", value: "GeometrySolid" },
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
        icon.source: "qrc:/images/toolbar/Selection/clear-selection.svg"
        icon.width: 16
        icon.height: 16
        icon.color: "transparent"
        text: qsTr("清除选择")
        flat: false
        onClicked: root.clearButtonClicked()
        opacity: enabled ? 1.0 : 0.6
    }
    CheckBox {
        id: angleSpreadCheckBox
        // 加大勾选区域，以实色背景区分已选状态；保留控件自身的键盘和无障碍行为。
        indicator: Rectangle {
            implicitWidth: 24
            implicitHeight: 24
            x: angleSpreadCheckBox.leftPadding
            y: angleSpreadCheckBox.topPadding + (angleSpreadCheckBox.availableHeight - height) / 2
            radius: Theme.radiusControl
            color: angleSpreadCheckBox.checked ? (angleSpreadCheckBox.enabled ? Theme.primaryPressed : Theme.textSecondary) : Theme.surface
            border.width: 2
            border.color: angleSpreadCheckBox.checked || angleSpreadCheckBox.visualFocus ? Theme.primaryPressed : Theme.textSecondary
            Shape {
                anchors.fill: parent
                visible: angleSpreadCheckBox.checked
                ShapePath {
                    strokeColor: Theme.textOnPrimary
                    strokeWidth: 3
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    startX: 5
                    startY: 12
                    PathLine { x: 10; y: 17 }
                    PathLine { x: 19; y: 7 }
                }
            }
        }
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
    /** type:string 选择框中当前文本 */
    property alias comboBoxSelectedString: selectModeComboBox.currentText
}
