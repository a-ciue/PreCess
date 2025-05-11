import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import fileLoader
import commands

Menu {
    id: commandMenu
    title: qsTr("命令")

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
