import QtQuick 2.15
import QtQuick.Controls

Item {
    id: root
    signal buttonPressed(int index,int type)
    ListView{
        id: objectListView
        anchors.fill: parent
        delegate:Row{
            Button{
                id:visibilityButton
                text: "隐藏"
                onClicked:{
                    root.buttonPressed(index,1)
                }
            }
            Button{
                id:deleteButton
                text: "删除"
                onClicked:{
                    root.buttonPressed(index,2)
                }
            }
            Text{
                id:objectName
                text: name
            }
            Rectangle{
                width:10
                height:10
                color: "red"
            }
        }
    }
    property alias objectModel: objectListView.model
}
