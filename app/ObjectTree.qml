import QtQuick 6.3
import QtQuick.Controls 2.15
import QtQuick.Controls.Basic
import QtQuick.Layouts 1.15

Pane{
	id:objectTree
	anchors.fill: parent

	TreeModel {
		id:treeModel
    }

	TreeView {
		id:treeView
		anchors.fill: parent
		model: treeModel
		columnSpacing: 0
		delegate: TreeViewDelegate {
			id:viewDelegate
			width: treeView.width
			height: 30
			
			readonly property real _padding: 5
            readonly property real _rowHeight: 25
            readonly property real _indentWidth: 20

			background: Rectangle {
                anchors.fill: parent
                color: {
                    if (viewDelegate.current) {
                        return "#e3f2fd"  // 选中状态 - 浅蓝色
                    } else if (viewDelegate.hovered) {
                        return "#f0f0f0"  // 悬停状态 - 浅灰色
                    } else {
                        return "transparent"
                    }
                }

                // 选中时的左边框
                Rectangle {
                    width: 3
                    height: parent.height
                    color: viewDelegate.current ? "#1976d2" : "transparent"
                    anchors.left: parent.left
                }
            }

            // 展开/折叠指示器
            indicator: Rectangle {
                id: indicatorItem
                x: viewDelegate._padding + viewDelegate.depth * viewDelegate._indentWidth
                y: (viewDelegate._rowHeight - implicitHeight) / 2

                implicitWidth: 16
                implicitHeight: 16
                color: "transparent"
                visible: viewDelegate.isTreeNode && viewDelegate.hasChildren
                z: 10  // 确保在最上层

                Text {
                    anchors.centerIn: parent
                    text: viewDelegate.expanded ? "▼" : "▶"
                    color: "#666666"  // 深灰色
                    font.pixelSize: 10

                    Behavior on rotation {
                        NumberAnimation { duration: 100 }
                    }
                }

                // 使用 TapHandler 而不是 MouseArea，避免事件冲突
                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    onTapped: {
                        viewDelegate.treeView.toggleExpanded(viewDelegate.row)
                    }
                }
            }

            // 内容区域 - 关键修复
            contentItem: Item {
                implicitHeight: viewDelegate._rowHeight
                implicitWidth: textContent.implicitWidth + viewDelegate._padding * 2

                Row {
                    id: textContent
                    anchors.fill: parent
                    // 关键修复：移除重复的深度缩进计算
                    // 只保留 padding + indicator 宽度，深度缩进由 indicator 的 x 坐标处理
                    anchors.leftMargin: viewDelegate._padding + (viewDelegate.hasChildren ? indicatorItem.implicitWidth + 2 : 0)
                    anchors.rightMargin: viewDelegate._padding
                    spacing: 4

                    // 节点名称
                    Text {
                        id: nameText
                        text: viewDelegate.model.name || "N/A"
                        color: viewDelegate.current ? "#1976d2" : "#333333"
                        font.pixelSize: 13
                        font.family: "Consolas"
                        font.weight: viewDelegate.current ? Font.Bold : Font.Normal
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                        height: parent.height
                    }

                    // 值/数据 - 关键：不占用额外空间
                    Text {
                        id: valueText
                        text: viewDelegate.model.number ? " (" + viewDelegate.model.number + ")" : ""
                        color: "#888888"
                        font.pixelSize: 11
                        visible: text !== ""
                        verticalAlignment: Text.AlignVCenter
                        height: parent.height
                    }
                }
            }

            // 左键点击选择 - 使用 TapHandler
            TapHandler {
                acceptedButtons: Qt.LeftButton
                onTapped: {
                    let index = viewDelegate.treeView.index(viewDelegate.row, viewDelegate.column)
                    viewDelegate.treeView.selectionModel.setCurrentIndex(index, ItemSelectionModel.ClearAndSelect)

                    let name = viewDelegate.model.name || "N/A"
                    let number = viewDelegate.model.number || ""
                    console.log("Selected:", name, number)
                }
            }

            // 右键菜单处理 - 简化版本
            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: (eventPoint) => {
                    // 先选中当前项
                    let index = viewDelegate.treeView.index(viewDelegate.row, viewDelegate.column)
                    viewDelegate.treeView.selectionModel.setCurrentIndex(index, ItemSelectionModel.ClearAndSelect)

                    // 方法2：直接使用场景坐标弹出
                    contextMenu.popup()
                }
            }

            // 右键上下文菜单 - 浅色主题
            Menu {
                id: contextMenu

                background: Rectangle {
                    color: "#ffffff"  // 白色背景
                    border.color: "#d0d0d0"  // 浅灰色边框
                    border.width: 1
                    radius: 4
                    implicitWidth: 120
                    implicitHeight: Math.min(contentItem.implicitHeight, 300)

                    // 使用简单的矩形阴影替代
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -2
                        z: -1
                        color: "transparent"
                        border.color: "#00000020"  // 半透明黑色边框
                        border.width: 2
                        radius: parent.radius
                    }
                }

                contentItem: Column {
                    id: menuColumn
                    spacing: 0

                    // 隐藏
                    MenuItem {
                        id: hideItem
                        text: "隐藏"
                        implicitWidth: 100
                        implicitHeight: 32
                        width: implicitWidth

                        background: Rectangle {
                            color: hideItem.hovered ? "#f0f0f0" : "transparent"  // 浅灰色悬停
                        }

                        contentItem: Text {
                            text: hideItem.text
                            color: hideItem.hovered ? "#1976d2" : "#333333"
                            font.pixelSize: 12
                            font.family: "Microsoft YaHei"
                            leftPadding: 15
                            rightPadding: 15
                            verticalAlignment: Text.AlignVCenter
                        }

                        onTriggered: {
                            let name = viewDelegate.model.name || "N/A"
                            console.log("隐藏:", name)
                            //具体实现

                        }
                    }

                    // 显示
                    MenuItem {
                        id: showItem
                        text: "显示"
                        implicitWidth: 100
                        implicitHeight: 32
                        width: implicitWidth

                        background: Rectangle {
                            color: showItem.hovered ? "#f0f0f0" : "transparent"
                        }

                        contentItem: Text {
                            text: showItem.text
                            color: showItem.hovered ? "#1976d2" : "#333333"
                            font.pixelSize: 12
                            font.family: "Microsoft YaHei"
                            leftPadding: 15
                            rightPadding: 15
                            verticalAlignment: Text.AlignVCenter
                        }

                        onTriggered: {
                            let name = viewDelegate.model.name || "N/A"
                            console.log("显示:", name)
                            //具体实现
                        }
                    }

                    MenuSeparator{}

                    // 删除
                    MenuItem {
                        id: deleteItem
                        text: "删除"
                        implicitWidth: 100
                        implicitHeight: 32
                        width: implicitWidth

                        background: Rectangle {
                            color: deleteItem.hovered ? "#ffebee" : "transparent"  // 浅红色悬停
                        }

                        contentItem: Text {
                            text: deleteItem.text
                            color: deleteItem.hovered ? "#d32f2f" : "#c62828"  // 红色系
                            font.pixelSize: 12
                            font.family: "Microsoft YaHei"
                            leftPadding: 15
                            rightPadding: 15
                            verticalAlignment: Text.AlignVCenter
                        }

                        onTriggered: {
                            let name = viewDelegate.model.name || "N/A"
                            console.log("删除:", name)

                            //删除逻辑实现
                            let modelIndex = viewDelegate.treeView.index(
                                            viewDelegate.row,
                                            viewDelegate.column)

                                if (!modelIndex.valid)
                                    return

                                let realRow = modelIndex.row
                                let parentIndex = modelIndex.parent

                                viewDelegate.treeView.model.removeNode(realRow, parentIndex)

                                // ✅ 清除选中状态
                                viewDelegate.treeView.selectionModel.clear()

                                // ✅ 关闭菜单
                                contextMenu.close()
                        }
                    }
                }
            }
		}

        selectionModel: ItemSelectionModel {}

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            background: Rectangle {
                implicitWidth: 8
                color: "transparent"
            }
            contentItem: Rectangle {
                implicitWidth: 8
                radius: 4
                color: parent.hovered ? "#c0c0c0" : "#e0e0e0"
            }
        }
	}
}