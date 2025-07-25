import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

import fileLoader
import commands
import systems.algo

Menu {
    id: commandMenu
    title: qsTr("算法")

    required property list<QAlgorithmInfo> algoInfos
    required property SideBar sideBar
    required property CommandDispatcher commandDispatcher

    Repeater {
        model: algoInfos
        MenuItem {
            text: modelData.display_name
            onTriggered: {
                sideBar.curAlgoInfo = modelData
            }
        }
    }
}
