import QtQuick

QtObject {
    id: root
    required property var target_signal
    property int listener_count

    function getListenerCount() {
        return listener_count
    }
    
    function registerSignalListener(callback) {
        listener_count += 1
        root.target_signal.connect(callback)
    }
    function unregisterSignalListener(callback) {
        listener_count -= 1
        root.target_signal.disconnect(callback)
    }
}