pragma Singleton
import QtQuick

QtObject {
    property QtObject selection: QtObject {
        property int activeModelId: -1
        property int activeComponentId: -1
        property string selectMode: ""
        property int listeningSelectorIndex: -1

        signal confirmed(var sel)
    }

    property var activeOperation: null
    property var registry: ({})

    signal modelVisibilityUpdated(int modelId, bool visible)
    signal componentVisibilityUpdated(int componentId, bool visible)
}
