/**
 * @file FileFeatureFlow.qml
 * @brief 文件选择和类型消歧；参数编辑及执行复用现有 SideBar 操作会话。
 */
import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts
import QtQuick.Dialogs
import app.core
import app.model

Item {
    id: root
    required property var featureSystem
    property var taskStatus: QModelManager.taskStatus
    property var query: QModelManager.query
    readonly property bool isWasm: Qt.platform.os === "wasm"
    readonly property var importTypes: featureSystem.importFileTypes || []
    readonly property var exportTypes: featureSystem.exportFileTypes || []
    readonly property bool choosingType: typePopup.visible
    readonly property bool choosingExportPath: isWasm && _candidates.length > 0 && _candidates[0].direction === "export"
    property var currentPaths: []
    property var _operation: null
    property var _candidates: []
    property var _dialogTypes: []
    signal parametersRequested

    function filters(types) {
        return types.map(type => type.display_name + " (" + (type.extensions.length ? type.extensions.map(ext => "*." + ext).join(" ") : "*") + ")");
    }

    function openImport() {
        if (taskStatus.writePending)
            return;
        cancel();
        if (!importTypes.length) {
            taskStatus.reportFailure(qsTr("没有可用的导入功能。"));
            return;
        }
        if (isWasm) {
            const accept = importTypes.some(type => !type.extensions.length) ? "" : importTypes.map(type => type.extensions.map(ext => "." + ext).join(",")).join(",");
            QWasmBridge.pickFile(accept, true);
            return;
        }
        _dialogTypes = Array.from(importTypes);
        importDialog.nameFilters = [qsTr("所有文件 (*)")].concat(filters(_dialogTypes));
        importDialog.selectedNameFilter.index = 0;
        importDialog.open();
    }

    function openExport(suggestedName) {
        if (taskStatus.writePending)
            return;
        cancel();
        if (!exportTypes.length) {
            taskStatus.reportFailure(qsTr("没有可用的导出功能。"));
            return;
        }
        _dialogTypes = Array.from(exportTypes);
        if (isWasm) {
            _candidates = _dialogTypes;
            typeCombo.currentIndex = 0;
            if (suggestedName)
                nameField.text = suggestedName;
            typePopup.open();
            return;
        }
        exportDialog.nameFilters = filters(_dialogTypes);
        if (suggestedName)
            exportDialog.selectedFile = exportDialog.currentFolder + "/" + suggestedName;
        exportDialog.open();
    }

    // 同一批文件共用一个 Feature 及其参数，扩展名仅缩小候选。
    function importFiles(urls, featureName) {
        if (taskStatus.writePending) {
            releaseFiles(Array.from(urls).map(url => App.localFilePath(url)));
            return;
        }
        cancel();
        chooseFile("import", Array.from(urls).map(url => App.localFilePath(url)), featureName);
    }

    function chooseFile(direction, files, featureName) {
        const paths = Array.isArray(files) ? files : [files];
        if (taskStatus.writePending) {
            if (direction === "import")
                releaseFiles(paths);
            cancel();
            return;
        }
        if (direction === "export")
            cancel();
        currentPaths = paths;
        const types = Array.from(direction === "import" ? importTypes : exportTypes);
        const explicitType = featureName ? types.find(type => type.feature === featureName) : null;
        if (featureName && !explicitType) {
            taskStatus.reportFailure(qsTr("文件功能已注销，请重新选择。"));
            if (direction === "import")
                releaseFiles(paths);
            cancel();
            return;
        }
        if (explicitType) {
            prepare(explicitType, paths);
            return;
        }
        const suffixes = paths.map(path => path.split(/[\\/]/).pop().split(".").pop().toLowerCase());
        const matching = types.filter(type => suffixes.every(suffix => type.extensions.some(ext => ext.toLowerCase() === suffix)));
        _candidates = matching.length ? matching : types;
        if (_candidates.length === 1) {
            prepare(_candidates[0], paths);
        } else if (_candidates.length) {
            typeCombo.currentIndex = 0;
            typePopup.open();
        } else {
            taskStatus.reportFailure(qsTr("没有可用的文件功能。"));
            if (direction === "import")
                releaseFiles(paths);
            cancel();
        }
    }

    function prepare(type, paths) {
        if (taskStatus.writePending) {
            if (type.direction === "import")
                releaseFiles(paths);
            cancel();
            return;
        }
        const name = type.feature;
        const info = featureSystem.featuresInfo.find(item => item.name === name);
        if (!info) {
            taskStatus.reportFailure(qsTr("文件功能已注销，请重新选择。"));
            if (type.direction === "import")
                releaseFiles(paths);
            cancel();
            return;
        }
        const multiple = info.arg_types[0].type === QArgType.Paths;
        if (!multiple && paths.length > 1) {
            taskStatus.reportFailure(qsTr("该功能只接受单个文件，请重新选择。"));
            releaseFiles(paths);
            return;
        }
        const operation = {
            isFeature: true,
            info: info,
            fileType: type,
            sourcePaths: paths,
            execute: function () {
                const outputPath = root.isWasm && type.direction === "export" ? root.featureSystem.getParameterValues(name)[0] : "";
                const result = root.featureSystem.invoke(name);
                if (type.direction === "import") {
                    if (App.registry.renderWindow)
                        App.registry.renderWindow.resetCamera();
                } else if (root.isWasm)
                    QWasmBridge.downloadFile(outputPath.split(/[\\/]/).pop(), outputPath);
                return result;
            }
        };
        const previous = root._operation;
        root._operation = operation;
        App.selection.listeningSelectorIndex = -1;
        App.activeOperation = operation;
        root.releaseOperation(previous);
        // 普通功能切换完成后，直接设置本次文件操作的参数。
        featureSystem.setParameter(name, 0, multiple ? paths : paths[0]);
        if (type.direction === "export") {
            const cid = App.selection.activeComponentId;
            const mid = App.selection.activeModelId;
            const ids = cid >= 0 && query.hasComponent(cid) ? [cid] : mid >= 0 ? query.getComponentsSummary(mid).map(item => item.component_id) : [];
            const selection = componentSelection.createObject(null);
            selection.setComponentIds(ids);
            featureSystem.setParameter(name, 1, selection);
        }
        root.parametersRequested();
    }

    function cancel() {
        if (choosingType && _candidates[0].direction === "import")
            releaseFiles(currentPaths);
        typePopup.close();
    }

    function releaseOperation(operation) {
        if (operation && operation.fileType.direction === "import")
            releaseFiles(operation.sourcePaths);
    }

    function releaseFiles(paths) {
        if (isWasm)
            QWasmBridge.releaseFiles(paths);
    }

    Connections {
        target: root.isWasm ? QWasmBridge : null
        function onFilesImported(paths) {
            root.importFiles(paths);
        }
    }
    Connections {
        target: root.featureSystem
        function onFeaturesInfoChanged() {
            if (root.choosingType)
                root.cancel();
            if (root._operation) {
                const type = root._operation.fileType;
                const types = type.direction === "import" ? root.featureSystem.importFileTypes : root.featureSystem.exportFileTypes;
                if (!types.some(item => item.feature === type.feature))
                    App.activeOperation = null;
            }
        }
    }
    Connections {
        target: App
        function onActiveOperationChanged() {
            if (App.activeOperation !== root._operation) {
                const previous = root._operation;
                root._operation = null;
                root.releaseOperation(previous);
                root.cancel();
            }
        }
    }
    Component {
        id: componentSelection
        QSelection {}
    }
    FileDialog {
        id: importDialog
        fileMode: FileDialog.OpenFiles
        onAccepted: {
            const type = root._dialogTypes[selectedNameFilter.index - 1];
            root.importFiles(selectedFiles, type ? type.feature : "");
        }
    }
    FileDialog {
        id: exportDialog
        fileMode: FileDialog.SaveFile
        onAccepted: {
            const type = root._dialogTypes[selectedNameFilter.index];
            if (type)
                root.chooseFile("export", App.localFilePath(selectedFile), type.feature);
        }
    }
    Popup {
        id: typePopup
        objectName: "fileTypePopup"
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        closePolicy: Popup.NoAutoClose
        width: Math.min(440, parent ? parent.width - 24 : 440)
        ColumnLayout {
            width: parent.width
            Label {
                text: root.choosingExportPath ? qsTr("导出模型") : qsTr("选择文件类型")
                font.bold: true
            }
            Label {
                visible: !root.choosingExportPath
                text: root.currentPaths.join("\n")
                wrapMode: Text.WrapAnywhere
                Layout.fillWidth: true
            }
            TextField {
                id: nameField
                objectName: "exportFileName"
                visible: root.choosingExportPath
                Layout.fillWidth: true
                text: "model.obj"
                placeholderText: qsTr("文件名")
            }
            ComboBox {
                id: typeCombo
                objectName: "fileTypeCombo"
                model: root._candidates
                textRole: "display_name"
                Layout.fillWidth: true
                onCurrentIndexChanged: {
                    if (!root.choosingExportPath)
                        return;
                    const type = root._candidates[currentIndex];
                    if (!type)
                        return;
                    const ext = type.extensions.length ? "." + type.extensions[0] : "";
                    const dot = nameField.text.lastIndexOf(".");
                    const base = dot > 0 ? nameField.text.slice(0, dot) : nameField.text;
                    if (base.length)
                        nameField.text = base + ext;
                }
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                Button {
                    text: qsTr("取消")
                    onClicked: root.cancel()
                }
                Button {
                    text: qsTr("下一步")
                    onClicked: {
                        const type = root._candidates[typeCombo.currentIndex];
                        let paths = root.currentPaths;
                        if (root.choosingExportPath) {
                            let file_name = nameField.text.trim();
                            if (!file_name.length)
                                return;
                            if (file_name.indexOf(".") < 0 && type.extensions.length)
                                file_name += "." + type.extensions[0];
                            paths = ["/tmp/" + file_name];
                        }
                        typePopup.close();
                        root.prepare(type, paths);
                    }
                }
            }
        }
    }
}
