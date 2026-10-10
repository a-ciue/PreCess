/**
 * @file OperationSession.qml
 * @brief 操作会话：协调子功能选择、注册表刷新、功能激活和参数模型。
 */
import QtQml
import app.core
import app.model
import "FeatureNavigation.js" as FeatureNavigation

QtObject {
    id: root

    property var activeOperation: null
    property var featureSystem: QModelManager.featureSystem
    property var featureInfos: root.featureSystem.featuresInfo
    readonly property var navigationDefinitions: root.featureSystem.navigationDefinitions
    readonly property var navigationEntries: FeatureNavigation.buildEntries(root.featureInfos, root.navigationDefinitions)
    readonly property bool isGroupedOperation: !!(root.activeOperation && root.activeOperation.entryId)
    readonly property var activeEntry: root.isGroupedOperation ? root.navigationEntries.find(entry => entry.id === root.activeOperation.entryId) || null : null
    readonly property var subFeatures: root.activeEntry ? root.activeEntry.items : (!root.isGroupedOperation && root.activeOperation && root.activeOperation.isFeature && root.activeOperation.info ? [root.activeOperation.info] : [])
    readonly property bool hasSubFeatureChoice: root.subFeatures.length > 1
    readonly property string panelTitle: root.activeEntry ? qsTr("操作面板 - %1").arg(root.activeEntry.title) : qsTr("操作面板")
    readonly property OperationParameterModel parameterModel: OperationParameterModel {
        featureSystem: root.featureSystem
        _acceptingControlEdits: !root._switchingOperation
    }

    property bool _switchingOperation: false
    property var _lastSubFeatures: ({})
    property string _parameterIdentity: ""
    property string _parameterSchema: ""

    // QtObject 没有默认对象列表，用显式属性承载随会话存活的信号连接。
    readonly property Connections _operationChanges: Connections {
        target: App
        function onActiveOperationChanged() {
            root.applyOperation(App.activeOperation);
        }
    }
    readonly property Connections _selectionChanges: Connections {
        target: App.selection
        function onSelectionInvalidated() {
            App.selection.listeningSelectorIndex = -1;
            root.parameterModel.clearSelections();
        }
    }

    onNavigationEntriesChanged: Qt.callLater(root.refreshRegisteredSubFeature)
    onSubFeaturesChanged: Qt.callLater(root.refreshSubFeature)
    Component.onCompleted: root.applyOperation(App.activeOperation)

    function applyOperation(operation) {
        const identity = operation && operation.isFeature && operation.info ? JSON.stringify([operation.entryId || "", operation.info.name]) : "";
        const schema = identity ? JSON.stringify([operation.info.arg_types.map(arg => [arg.name, arg.type, arg.content]), operation.categoryId && operation.info.category_defaults ? operation.info.category_defaults[operation.categoryId] : null, operation.defaultParameters]) : "";
        const preserveParameters = identity !== "" && identity === root._parameterIdentity && schema === root._parameterSchema;
        // 切换模型时控件可能同步更新文本；此期间只同步快照，不接受控件反向写入。
        root._switchingOperation = true;
        try {
            // 描述包装可以刷新；只有操作身份或参数声明变化才重新初始化会话。
            root.activeOperation = operation;
            // 同名替换会退出旧功能；激活协调不能随参数保留一起跳过。
            const featureName = operation && operation.isFeature && operation.info ? operation.info.name : "";
            root.parameterModel.featureSystem.setFeatureActive(featureName);
            if (!preserveParameters) {
                App.selection.listeningSelectorIndex = -1;
                root.parameterModel.reset(operation);
            } else
                root.parameterModel.argumentTypes = operation.info.arg_types;
            root._parameterIdentity = identity;
            root._parameterSchema = schema;
        } finally {
            root._switchingOperation = false;
        }
        if (root.isGroupedOperation && !root.activeOperation.info)
            Qt.callLater(root.refreshSubFeature);
    }

    function selectSubFeature(index, refresh = false) {
        if (!root.isGroupedOperation)
            return;
        const entry = root.activeEntry;
        if (!entry) {
            App.activeOperation = null;
            return;
        }
        const info = index >= 0 && index < root.subFeatures.length ? root.subFeatures[index] : null;
        // QObject 的 JS 包装引用不稳定；普通重复点击按注册名称判定幂等。
        if (!refresh && info && root.activeOperation.info && root.activeOperation.info.name === info.name)
            return;
        if (!refresh && !info && !root.activeOperation.info)
            return;
        if (info)
            root._lastSubFeatures[entry.id] = info.name;
        App.activeOperation = {
            entryId: entry.id,
            isFeature: !!info,
            categoryId: entry.categoryId,
            categoryTitle: entry.title,
            info: info,
            execute: info ? function () {
                return root.featureSystem.invoke(info.name);
            } : null
        };
    }

    function refreshRegisteredSubFeature() {
        root.refreshSubFeature(true);
    }

    function refreshSubFeature(refresh = false) {
        if (!root.isGroupedOperation)
            return;
        if (!root.activeEntry) {
            // 父入口消失后，不再持有已卸载子功能的闭包或选择监听。
            App.activeOperation = null;
            return;
        }
        const name = root.activeOperation.info ? root.activeOperation.info.name : root._lastSubFeatures[root.activeOperation.entryId];
        const index = root.subFeatures.findIndex(info => info.name === name);
        if (root.subFeatures.length > 0)
            root.selectSubFeature(index >= 0 ? index : 0, refresh);
        else
            root.selectSubFeature(-1, refresh);
    }
}
