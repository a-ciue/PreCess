/**
 * @file QObjectList.qml
 * @brief 对象列表，用列表方式显示当前加载的模型并提供删除、隐藏和重命名功能
 */

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
    /**
     * @brief 对象显示列表，每行由两个按钮，一个文本和一个图标组成
     */
    ListView{
        id: objectListView
        anchors.fill: parent
        model:ListModel{}
        delegate:Row{
            Button{
                id:visibilityButton
                text: "隐藏"
                onClicked:{
                    //root.buttonPressed(index,1)
                    changeModelVisibility(name, !checked)
                }
                checkable: true
            }
            Button{
                id:deleteButton
                text: "删除"
                onClicked:{
                    //root.buttonPressed(index,2)
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
    /**
     * @brief 为列表添加一行模型信息
     * @param modelName 模型名字
     */
    function addItem(modelName){
        objectModel.append({"name":modelName})
        idxMap[modelName] = objectModel.count - 1
        console.log("addIdx", idxMap[modelName])
    }
    /**
     * @brief 删除一行模型信息
     * @param modelName 模型名字
     */
    function removeItem(modelName){
        console.log("remove: ", modelName)
        objectModel.remove(idxMap[modelName])
        delete idxMap[modelName]
    }
    /**
     * @brief 重命名一行模型信息的名字
     * @param oldName 模型的旧名
     * @param newName 模型的新名
     */
    //function renameItem(oldName, newName){}
    /** type:var 对象列表的model数据构造 */
    property alias objectModel: objectListView.model
}
