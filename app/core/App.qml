pragma Singleton
import QtQuick

QtObject {
    property QtObject selection: QtObject {
        property int activeModelId: -1
        property int activeComponentId: -1
        property string selectMode: "None"
        property int listeningSelectorIndex: -1
        // 切换参数或结束选择时递增，丢弃旧渲染任务排队中的选择结果。
        property int selectionRevision: 0
        onListeningSelectorIndexChanged: ++selectionRevision
        signal selectionUpdated(var sel)
        signal selectionInvalidated()
    }

    property var activeOperation: null
    property bool importDragActive: false
    // 注册表对象保持稳定，各控件引用通过属性变更通知各自的绑定。
    readonly property QtObject registry: QtObject {
        property var renderWindow: null
        property var treeModel: null
        property var objectTree: null
    }

    signal modelVisibilityUpdated(int modelId, bool visible)
    signal componentVisibilityUpdated(int componentId, bool meshVisible, bool geometryVisible)
}
