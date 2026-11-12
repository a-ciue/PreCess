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
    property var registry: ({})

    function localFilePath(url) {
        const text = String(url)
        if (!text.startsWith("file://"))
            return text
        let path = decodeURIComponent(text.slice(7))
        if (!path.startsWith("/"))
            return "//" + path
        return /^\/[A-Za-z]:\//.test(path) ? path.slice(1) : path
    }
}
