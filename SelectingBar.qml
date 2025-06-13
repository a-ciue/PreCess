/**
 * @file QSelectingBar.qml
 * @brief 功能选择栏，用户选择使用功能的交互界面，提供一栏功能按钮
 */

import QtQuick
import QtQuick.Controls 6.7
import QtQuick.Layouts

pragma ComponentBehavior: Bound
Item{
    signal modeButtonClicked(Button a,bool ischecked)
    signal buttonFunction(int index,int modeOrConfirm)
    signal buttonGroupFunction
    signal changeEdgeRender()

    function resetChecked(){
        modeButtonGroup.checkState = Qt.Unchecked
    }

    RowLayout{
        anchors.fill: parent
        spacing: 3

        Button{
            id: edgeRenderGroup
            text: "边渲染"
            checked:edgeRenderCheck
            onClicked: {
                changeEdgeRender()
            }
        }
        ToolSeparator {}

        Repeater{
            id: modeButton
            delegate: Button{
                text: name
                required property string name
                required property int index
                function getIndex(){ return index }
                function getchecked(){ return checked }

                onClicked: {
                    buttonFunction(index,0)
                }

                Component.onCompleted:{
                    //modeButtonClicked(this,getchecked())
                }
            }
        }
        Item{
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        Repeater{
            id: confirmButton
            delegate: Button{
                required property int index
                required property string name
                
                text: name
                
                onClicked: {
                    buttonFunction(index, 1)
                    modeButton.itemAt(index).toggle()
                }
            }
        }
        Item{
            Layout.preferredWidth: 20
            Layout.fillHeight: true
        }
    }
    onModeButtonClicked:(a, ischecked)=>{
        //confirmButton.itemAt(a.getIndex()).visible = ischecked
    }

    /** type:var 选择按钮的model数据构造 */
    property alias modeButtonModel :modeButton.model
    /** type:var 确认按钮的model数据构造 */
    property alias confirmButtonModel :confirmButton.model
}
