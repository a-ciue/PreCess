pragma Singleton
import QtQuick

QtObject {
    property QtObject selection: QtObject {
        property int activeModelId: -1
        property string selectMode: ""
        property int listeningSelectorIndex: -1

        signal confirmed(var sel)
    }

    property var activeOperation: null

    signal modelVisibilityUpdated(int modelId, bool visible)

    property var _modelVisibility: ({})

    function setModelVisible(modelId, visible) {
        _modelVisibility[modelId] = visible
        modelVisibilityUpdated(modelId, visible)
    }

    function isModelVisible(modelId) {
        return _modelVisibility[modelId] !== undefined ? _modelVisibility[modelId] : true
    }
}
