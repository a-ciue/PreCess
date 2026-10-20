/**
 * @file WallpaperBackground.qml
 * @brief 主窗口壁纸背景：双图淡入交接、清除与加载失败回退
 *
 * 当前选择由外部 source 决定；两张 Image 只承担切换动画。加载失败时停止过渡、
 * 清空展示并露出纯色底，同时发出 loadFailed 由应用侧记录；组件不写设置、不删文件。
 */

import QtQuick

Item {
    id: root

    //! @brief 当前选择的壁纸源（空串表示纯色背景）
    property url source
    //! @brief 无壁纸或加载失败时的底色
    property color backgroundColor
    //! @brief 壁纸色调遮罩
    property color tintColor
    //! @brief 淡入时长
    property int fadeDuration: 400
    //! @brief 当前选择是否加载失败
    readonly property bool failed: _failed

    //! @brief 选择的壁纸加载失败
    signal loadFailed(url imageSource)

    property bool _failed: false

    Rectangle {
        anchors.fill: parent
        color: root.backgroundColor
    }

    Image {
        id: currentImage
        objectName: "wallpaperCurrent"
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        visible: source.toString() !== ""

        onStatusChanged: {
            if (status === Image.Error && source.toString() === root.source.toString() && incomingImage.source.toString() === "") {
                root.fail();
            }
        }
    }

    Image {
        id: incomingImage
        objectName: "wallpaperIncoming"
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        visible: source.toString() !== ""
        opacity: 0

        onStatusChanged: {
            if (root.source.toString() === "" || source.toString() !== root.source.toString())
                return;
            if (status === Image.Ready)
                fadeIn.restart();
            else if (status === Image.Error)
                root.fail();
        }
    }

    Rectangle {
        objectName: "wallpaperTint"
        anchors.fill: parent
        color: root.tintColor
        visible: root.source.toString() !== "" && !root.failed

        Behavior on color {
            ColorAnimation {
                duration: 300
                easing.type: Easing.InOutCubic
            }
        }
    }

    NumberAnimation {
        id: fadeIn
        target: incomingImage
        property: "opacity"
        from: 0
        to: 1
        duration: root.fadeDuration
        easing.type: Easing.InOutCubic

        onFinished: {
            const next = incomingImage.source;
            currentImage.source = next;
            incomingImage.source = "";
            incomingImage.opacity = 0;
        }
    }

    function clearImages() {
        fadeIn.stop();
        currentImage.source = "";
        incomingImage.source = "";
        incomingImage.opacity = 0;
    }

    function fail() {
        if (failed)
            return;
        _failed = true;
        const failedSource = source;
        clearImages();
        loadFailed(failedSource);
    }

    function applySource() {
        const next = source.toString();
        if (next === "") {
            _failed = false;
            clearImages();
            return;
        }
        if (currentImage.source.toString() === next) {
            _failed = false;
            fadeIn.stop();
            incomingImage.source = "";
            incomingImage.opacity = 0;
            return;
        }
        if (currentImage.source.toString() === "") {
            // 初次加载或失败后重新选择：直接显示，不做淡入
            _failed = false;
            incomingImage.source = "";
            incomingImage.opacity = 0;
            currentImage.source = source;
            return;
        }
        fadeIn.stop();
        _failed = false;
        incomingImage.opacity = 0;
        incomingImage.source = source;
    }

    onSourceChanged: applySource()
    Component.onCompleted: applySource()
}
