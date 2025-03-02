import QtQuick 2.15
import QtQuick.Controls

Item {
    id: root
    signal buttonPressed(int index,int type)
    // call Model
    signal removeModel(string modelName)
    signal renameModel(string modelName)
    signal changeModelVisibility(string modelName,bool visibility)
    property var idxMap: ({})
    ListView{
        id: objectListView
        anchors.fill: parent
        model:ListModel{}
        delegate:Row{
            Button{
                id:visibilityButton
                text: "隐藏"
                onClicked:{
                    root.buttonPressed(index,1)
                    changeModelVisibility(name)
                }
                checkable: true
            }
            Button{
                id:deleteButton
                text: "删除"
                onClicked:{
                    root.buttonPressed(index,2)
                    removeModel(name)
                    console.log("buttonDelName: ", name)
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
    // do
    function addItem(modelName){
        objectModel.append({"name":modelName})
        idxMap[modelName] = objectModel.count - 1
        console.log("addIdx", idxMap[modelName])
    }
    function removeItem(modelName){
        console.log("remove: ", modelName)
        objectModel.remove(idxMap[modelName])
        delete idxMap[modelName]
    }
    //function renameItem(oldName, newName){}
    property alias objectModel: objectListView.model
}
