import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

import fileLoader
import commands

Menu {
    id: commandMenu
    title: qsTr("算法")

    required property list<QCommand> commands
    required property SideBar sideBar
    required property CommandDispatcher commandDispatcher

    Repeater {
        model: commands
        MenuItem {
            text: modelData.name()
            onTriggered: {
                sideBar.curCommand = modelData
            }
        }
    }
}
