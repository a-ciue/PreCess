pragma Singleton
import QtQuick

QtObject {
    property QtObject selection: QtObject {
        property int activeModelId: -1
        property int activeComponentId: -1
        property string selectMode: "None"
        property int listeningSelectorIndex: -1
        signal confirmed(var sel)
        signal selectionInvalidated()
    }

    property var activeOperation: null
    property var registry: ({})

    //! @brief 全局可复用弹窗：显示一段标题+文本（实例注册于 Main.qml）
    function showDialog(title, text) {
        if (registry.appDialog)
            registry.appDialog.openWith(title, text)
    }

    signal modelVisibilityUpdated(int modelId, bool visible)
    signal componentVisibilityUpdated(int componentId, bool meshVisible, bool geometryVisible)
}
