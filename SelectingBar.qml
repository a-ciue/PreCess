import QtQuick
import QtQuick.Controls 6.7
import QtQuick.Layouts

pragma ComponentBehavior: Bound
Item{
    signal modeButtonClicked(Button a,bool ischecked)
    signal buttonFunction(int index,int modeOrConfirm)
    signal buttonGroupFunction
    signal changeEdgeRender(bool check)

    function resetChecked(){
        modeButtonGroup.checkState = Qt.Unchecked
    }

    RowLayout{
        anchors.fill: parent
        spacing: 3
        ButtonGroup{
            id:modeButtonGroup
            onCheckStateChanged: {
                if(checkState == Qt.Unchecked)
                    buttonGroupFunction()
            }
        }

        Button{
            id:edgeRenderGroup
            text: "边渲染"
            onClicked:{
                toggle()
                changeEdgeRender(checked)
            }
        }
        Rectangle {
            color: "black"
            Layout.preferredWidth: 1
            Layout.fillHeight: true
        }

        // Item{
        //     Layout.preferredWidth:10
        //     Layout.fillHeight: true
        // }

        Repeater{
            id: modeButton
            delegate: Button{
                text: name
                ButtonGroup.group: modeButtonGroup
                required property string name
                required property int index
                function getIndex(){ return index }
                function getchecked(){ return checked }

                onClicked: {
                    toggle()
                }
                onCheckedChanged: {
                    modeButtonClicked(this, getchecked())
                    if (checked) buttonFunction(index,0 )
                }

                Component.onCompleted:{
                    modeButtonClicked(this,getchecked())
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
        confirmButton.itemAt(a.getIndex()).visible = ischecked
    }

    //通过为两个model添加别名使之能被外界分别赋值
    property alias modeButtonModel :modeButton.model
    property alias confirmButtonModel :confirmButton.model
}
