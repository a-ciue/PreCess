/**
 * @file ObjectList.qml
 * @brief 对象列表，用列表方式显示当前加载的模型并提供删除、隐藏和重命名功能
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import app.core
import app.model

Pane {
    id: root

    ObjectListModel {
        id: objectModel
    }
    /**
     * @brief 对象显示列表，每行由两个按钮，一个文本和一个图标组成
     */
    ListView{
        id: objectListView
        anchors.fill: parent
        model: objectModel
        delegate: RowLayout {
            required property string name
            required property int savedId
            width: objectListView.width

            Button{
                id:visibilityButton
                text: "隐藏"
                onClicked:{
                    App.setModelVisible(savedId, !checked)
                }
                checkable: true
            }
            Button{
                id:deleteButton
                text: "删除"
                onClicked:{
                    QModelManager.removeModel(savedId)
                    console.log("buttonDelName: ", name)
                }
            }
            TextInput{
                id:objectName
                property string savedName: name
                Layout.fillWidth: true
                wrapMode: TextInput.WrapAnywhere
                font.bold: App.selection.activeModelId === savedId  // 根据选中状态设置粗体
                
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (App.selection.activeModelId === savedId) {
                            App.selection.activeModelId = -1  // 取消选中
                        } else {
                            App.selection.activeModelId = savedId  // 选中当前行
                        }
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
        if (App.selection.activeModelId === model_id) {
            App.selection.activeModelId = -1  // 如果删除的是当前选中的模型，重置选中状态
        }
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

    Component.onCompleted: {
        QModelManager.observer.modelAdded.connect(function(model_id) {
            addItem(model_id, QModelManager.query.getModelName(model_id))
        })

        QModelManager.observer.modelRemoved.connect(function(model_id) {
            removeItem(model_id)
        })
    }
}
