/**
 * @file DockIndicators.qml
 * @brief 拖放落点高亮（Classic 指示器的可视部分）
 */

import QtQuick

Item {
    id: root

    //! @brief 关联的 IndicatorsView（C++）
    property var indicatorView: null

    visible: indicatorView ? indicatorView.active : false

    Rectangle {
        x: root.indicatorView ? root.indicatorView.highlightX : 0
        y: root.indicatorView ? root.indicatorView.highlightY : 0
        width: root.indicatorView ? root.indicatorView.highlightWidth : 0
        height: root.indicatorView ? root.indicatorView.highlightHeight : 0
        radius: 3
        color: "#4a90e2"
        opacity: 0.85

        Rectangle {
            anchors.centerIn: parent
            width: Math.max(4, parent.width / 3)
            height: width
            radius: 1
            color: "#ffffff"
            opacity: 0.9
        }
    }
}
