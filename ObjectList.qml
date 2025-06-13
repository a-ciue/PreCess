/**
 * @file QObjectList.qml
 * @brief 对象列表，用列表方式显示当前加载的模型并提供删除、隐藏和重命名功能
 */

import QtQuick 2.15
import QtQuick.Controls

Pane {
    id: root
    signal removeModel(string model_id)
    signal renameModel(string oldName, string newName)
    signal changeModelVisibility(string model_id,bool visibility)
    signal selectionChanged(string modelName)
    property var idxMap: ({})
    property var selectedModel_id: "-1"  // 存储当前选中的模型名
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
                    changeModelVisibility(savedId, !checked)
                }
                checkable: true
            }
            Button{
                id:deleteButton
                text: "删除"
                onClicked:{
                    removeModel(savedId)
                    console.log("buttonDelName: ", name)
                }
            }
            TextInput{
                id:objectName
                property string savedName: name
                width: 80
                //text: name
                font.bold: root.selectedModel_id === savedId  // 根据选中状态设置粗体
                
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (root.selectedModel_id === savedId) {
                            root.selectedModel_id = "-1"  // 取消选中
                        } else {
                            root.selectedModel_id = savedId  // 选中当前行
                        }
                        root.selectionChanged(root.selectedModel_id)  // 发送信号
                    }
                    onDoubleClicked: {
                        objectName.forceActiveFocus()  // 强制获取焦点，进入编辑模式
                    }
                }
                
                onEditingFinished: {
                    if(text !== savedName){
                        renameModel(savedId, text)
                        console.log("模型名变更",savedName," -> ",text)
                        renameItem(savedName, text)
                        savedName = text
                    }
                    objectName.focus = false  // 添加这行，让控件失去焦点
                }

                Component.onCompleted: {
                    text = savedName
                }
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
    function addItem(model_id,modelName){
        objectModel.append({name:modelName,savedId:model_id})    //这行命令会为新增控件自动构造两个变量：name和savedId，可在控件中直接使用
        idxMap[model_id] = objectModel.count - 1
        console.log("addIdx", idxMap[model_id])
    }
    /**
     * @brief 删除一行模型信息
     * @param modelName 模型名字
     */
    function removeItem(model_id){
        console.log("remove: ", model_id)
        objectModel.remove(idxMap[model_id])
        delete idxMap[model_id]
    }
    /**
     * @brief 重命名一行模型信息的名字
     *
     * 将对象列表的构造model用idxMap里存的序数更新，之后在将idxMap里的模型名更新，最后将idxMap里的旧数据删除，因为是无序键值对所以无需考虑顺序
     * @param oldName 模型的旧名
     * @param newName 模型的新名
     */
    function renameItem(oldName, newName){
        objectModel.setProperty(idxMap[oldName],"name",newName)
        idxMap[newName] = idxMap[oldName]
        delete idxMap[oldName]
    }

    /** type:var 对象列表的model数据构造 */
    property alias objectModel: objectListView.model
    property alias curModelId: root.selectedModel_id
}
