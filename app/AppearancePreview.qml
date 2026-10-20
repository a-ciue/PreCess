/**
 * @file AppearancePreview.qml
 * @brief 外观预览：按当前主题与壁纸绘制的迷你工作区
 *
 * 预览直接复用 Theme 令牌，而非固定的示意配色；主题切换后整幅标本随之过渡，
 * 壁纸与遮罩与主窗口保持一致，便于在偏好设置内即时确认最终效果。
 */

import QtQuick
import QtQuick.Shapes

Item {
    id: root

    //! @brief 预览使用的壁纸源（空串表示主题纯色背景）
    property url wallpaperSource
    readonly property bool hasWallpaper: wallpaperSource != ""

    implicitHeight: Math.max(150, Math.min(230, Math.round(width * 0.5625)))
    Accessible.ignored: true

    // 各面统一使用同一段颜色过渡，避免时长/缓动在多处复制后漂移
    component SurfaceTransition: ColorAnimation {
        duration: 220
    }

    Rectangle {
        id: frame
        anchors.fill: parent
        radius: Theme.radiusMenu
        clip: true
        color: Theme.windowBackground
        border.width: 1
        border.color: Theme.border

        Behavior on color {
            SurfaceTransition {}
        }

        Image {
            id: wallpaperImage
            anchors.fill: parent
            source: root.wallpaperSource
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            visible: root.hasWallpaper && status !== Image.Error
        }

        Rectangle {
            anchors.fill: parent
            color: Theme.wallpaperTint
            visible: root.hasWallpaper && wallpaperImage.status !== Image.Error

            Behavior on color {
                SurfaceTransition {}
            }
        }

        // 迷你标题条
        Rectangle {
            id: titleBar
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
            }
            height: 22
            color: Theme.panelHeaderSurface

            Behavior on color {
                SurfaceTransition {}
            }

            Rectangle {
                id: mark
                anchors {
                    left: parent.left
                    leftMargin: 8
                    verticalCenter: parent.verticalCenter
                }
                width: 7
                height: 7
                radius: 2
                color: Theme.primary
            }

            Row {
                anchors {
                    left: mark.right
                    leftMargin: 6
                    verticalCenter: parent.verticalCenter
                }
                spacing: 4

                Rectangle {
                    width: 34
                    height: 3
                    radius: 1.5
                    color: Theme.textSecondary
                    opacity: 0.45
                }
                Rectangle {
                    width: 18
                    height: 3
                    radius: 1.5
                    color: Theme.textSecondary
                    opacity: 0.25
                }
            }

            Row {
                anchors {
                    right: parent.right
                    rightMargin: 8
                    verticalCenter: parent.verticalCenter
                }
                spacing: 4

                Repeater {
                    model: 3
                    Rectangle {
                        width: 4
                        height: 4
                        radius: 2
                        color: Theme.textSecondary
                        opacity: 0.35
                    }
                }
            }
        }

        // 迷你状态栏
        Rectangle {
            id: statusBar
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            height: 16
            color: Theme.surfaceAlt

            Behavior on color {
                SurfaceTransition {}
            }

            Rectangle {
                anchors {
                    left: parent.left
                    leftMargin: 8
                    verticalCenter: parent.verticalCenter
                }
                width: 42
                height: 3
                radius: 1.5
                color: Theme.textSecondary
                opacity: 0.35
            }

            Rectangle {
                anchors {
                    right: parent.right
                    rightMargin: 8
                    verticalCenter: parent.verticalCenter
                }
                width: 5
                height: 5
                radius: 2.5
                color: Theme.primary
                opacity: 0.85
            }
        }

        // 迷你左侧导航
        Rectangle {
            id: rail
            anchors {
                left: parent.left
                top: titleBar.bottom
                bottom: statusBar.top
            }
            width: Math.max(42, Math.round(frame.width * 0.16))
            color: Theme.surfaceAlt

            Behavior on color {
                SurfaceTransition {}
            }

            Rectangle {
                anchors {
                    right: parent.right
                    top: parent.top
                    bottom: parent.bottom
                }
                width: 1
                color: Theme.border
                opacity: 0.7
            }

            Column {
                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                    leftMargin: 5
                    rightMargin: 5
                    topMargin: 7
                }
                spacing: 4

                Repeater {
                    model: 4

                    Item {
                        width: rail.width - 10
                        height: 14

                        Rectangle {
                            anchors.fill: parent
                            radius: Theme.radiusControl
                            color: index === 0 ? Theme.primaryTint : "transparent"
                        }

                        Rectangle {
                            anchors {
                                left: parent.left
                                leftMargin: 4
                                verticalCenter: parent.verticalCenter
                            }
                            width: 2
                            height: 8
                            radius: 1
                            color: index === 0 ? Theme.primary : "transparent"
                        }

                        Rectangle {
                            anchors {
                                left: parent.left
                                leftMargin: 9
                                verticalCenter: parent.verticalCenter
                            }
                            width: Math.round((rail.width - 26) * (index === 0 ? 0.72 : 0.56))
                            height: 3
                            radius: 1.5
                            color: index === 0 ? Theme.primary : Theme.textSecondary
                            opacity: index === 0 ? 0.9 : 0.3
                        }
                    }
                }
            }
        }

        // 迷你右侧属性面板
        Rectangle {
            id: inspector
            anchors {
                right: parent.right
                top: titleBar.bottom
                bottom: statusBar.top
            }
            width: Math.max(52, Math.round(frame.width * 0.2))
            color: Theme.surface

            Behavior on color {
                SurfaceTransition {}
            }

            Rectangle {
                anchors {
                    left: parent.left
                    top: parent.top
                    bottom: parent.bottom
                }
                width: 1
                color: Theme.border
                opacity: 0.7
            }

            Column {
                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                    leftMargin: 8
                    rightMargin: 8
                    topMargin: 9
                }
                spacing: 8

                Rectangle {
                    width: Math.round(inspector.width * 0.5)
                    height: 3
                    radius: 1.5
                    color: Theme.textSecondary
                    opacity: 0.45
                }
                Rectangle {
                    width: Math.max(12, inspector.width - 26)
                    height: 3
                    radius: 1.5
                    color: Theme.textSecondary
                    opacity: 0.22
                }
                Rectangle {
                    width: Math.round(inspector.width * 0.62)
                    height: 3
                    radius: 1.5
                    color: Theme.textSecondary
                    opacity: 0.22
                }

                Item {
                    width: Math.round(inspector.width * 0.72)
                    height: 9

                    Rectangle {
                        anchors {
                            left: parent.left
                            right: parent.right
                            verticalCenter: parent.verticalCenter
                        }
                        height: 2
                        radius: 1
                        color: Theme.border
                    }

                    Rectangle {
                        anchors {
                            left: parent.left
                            verticalCenter: parent.verticalCenter
                        }
                        width: Math.round(parent.width * 0.62)
                        height: 2
                        radius: 1
                        color: Theme.primary
                        opacity: 0.7
                    }

                    Rectangle {
                        x: Math.round(parent.width * 0.62) - 3
                        anchors.verticalCenter: parent.verticalCenter
                        width: 7
                        height: 7
                        radius: 3.5
                        color: Theme.menuSurface
                        border.width: 1
                        border.color: Theme.primary
                    }
                }
            }
        }

        // 迷你视口：点阵与线框网格
        Item {
            id: viewport
            anchors {
                left: rail.right
                right: inspector.left
                top: titleBar.bottom
                bottom: statusBar.top
            }

            Grid {
                anchors.centerIn: parent
                columns: 7
                rows: 4
                spacing: 16

                Repeater {
                    model: 28
                    Rectangle {
                        width: 2
                        height: 2
                        radius: 1
                        color: Theme.textSecondary
                        opacity: 0.18
                    }
                }
            }

            Shape {
                anchors.centerIn: parent
                width: 120
                height: 120
                scale: Math.max(0.4, Math.min(1, Math.min(viewport.width / 240, viewport.height / 180)))

                // 顶面
                ShapePath {
                    strokeColor: Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.6)
                    strokeWidth: 1.2
                    fillColor: Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.1)
                    startX: 60
                    startY: 16
                    PathLine {
                        x: 96
                        y: 36
                    }
                    PathLine {
                        x: 60
                        y: 56
                    }
                    PathLine {
                        x: 24
                        y: 36
                    }
                    PathLine {
                        x: 60
                        y: 16
                    }
                }

                // 左面
                ShapePath {
                    strokeColor: Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.45)
                    strokeWidth: 1
                    fillColor: Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.05)
                    startX: 24
                    startY: 36
                    PathLine {
                        x: 60
                        y: 56
                    }
                    PathLine {
                        x: 60
                        y: 100
                    }
                    PathLine {
                        x: 24
                        y: 80
                    }
                    PathLine {
                        x: 24
                        y: 36
                    }
                }

                // 右面
                ShapePath {
                    strokeColor: Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.45)
                    strokeWidth: 1
                    fillColor: Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.07)
                    startX: 60
                    startY: 56
                    PathLine {
                        x: 96
                        y: 36
                    }
                    PathLine {
                        x: 96
                        y: 80
                    }
                    PathLine {
                        x: 60
                        y: 100
                    }
                    PathLine {
                        x: 60
                        y: 56
                    }
                }

                // 三角网格细分
                ShapePath {
                    strokeColor: Qt.rgba(Theme.textSecondary.r, Theme.textSecondary.g, Theme.textSecondary.b, 0.28)
                    strokeWidth: 0.7
                    fillColor: "transparent"
                    PathSvg {
                        path: "M60 36L60 16M60 36L96 36M60 36L60 56M60 36L24 36M42 68L24 36M42 68L60 56M42 68L60 100M42 68L24 80M78 68L60 56M78 68L96 36M78 68L96 80M78 68L60 100"
                    }
                }
            }
        }
    }
}
