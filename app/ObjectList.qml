/**
 * @file QObjectList.qml
 * @brief 对象列表，用列表方式显示当前加载的模型并提供删除、隐藏和重命名功能
 */

import QtQuick 2.15
import QtQuick.Layouts
import QtQuick.Controls

Pane {
    id: root
    signal removeModel(int model_id)
    signal renameModel(int modelId, string newName)
    signal changeModelVisibility(int model_id,bool visibility)
    signal selectionChanged(int modelId)
    property int selectedModel_id: -1  // 存储当前选中的模型id

    ObjectListModel {
        id: objectInitializeModel
    }
    /**
     * @brief 对象显示列表，每行由两个按钮，一个文本和一个图标组成
     */
    ListView{
        id: objectListView
        anchors.fill: parent
        model: objectInitializeModel
        delegate: RowLayout {
            required property string name
            required property int savedId
            width: objectListView.width

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
                Layout.fillWidth: true
                wrapMode: TextInput.WrapAnywhere
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
                        savedName = text
                    }
                    objectName.focus = false  // 添加这行，让控件失去焦点
                }

                Component.onCompleted: {
                    text = savedName
                }
            }
        }
    }
    /**
     * @brief 为列表添加一行模型信息
     * @param modelName 模型名字
     */
    function addItem(model_id,modelName){
        objectModel.addItem(model_id, modelName)
    }
    /**
     * @brief 删除一行模型信息
     * @param model_id 要删除的模型id
     */
    function removeItem(model_id){
        objectModel.removeItem(model_id)
    }
    /**
     * @brief 重命名一行模型信息的名字
     *
     * 将对象列表的构造model用idxMap里存的序数更新，之后在将idxMap里的模型名更新，最后将idxMap里的旧数据删除，因为是无序键值对所以无需考虑顺序
     * @param oldName 模型的旧名
     * @param newName 模型的新名
     */
    function renameItem(oldName, newName){
        objectModel.rename(oldName, newName)
    }

    /** type:var 对象列表的model数据构造 */
    property alias objectModel: objectListView.model
    property alias curModelId: root.selectedModel_id
}
