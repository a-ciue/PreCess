import QtQuick
import QtQuick.Controls

ListModel{
    id: root

    function addItem(modelId, modelName){
        append({name:modelName, savedId:modelId})
    }
    function removeItem(modelId){
        console.log("remove: ", modelId)
        let index = getIndex(modelId)
        remove(index)
    }
    function renameItem(modelId, newName){
        let index = getIndex(modelId)
        setProperty(index, "name", newName)
    }

    function getIndex(modelId){
        let foundIdx = -1
        for(let j=0; j< count; j++){
            let curId = get(j).savedId
            if(modelId === curId){
                foundIdx = j
            }
        }
        return foundIdx
    }
}