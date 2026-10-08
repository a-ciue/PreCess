/**
 * @file OperationSession.qml
 * @brief 操作会话：协调算法选择、注册表刷新、功能激活和参数模型。
 */
import QtQml
import app.core
import app.model
import "AlgorithmNavigation.js" as AlgorithmNavigation

QtObject {
    id: root

    property var activeOperation: null
    property var algorithmSystem: QModelManager.algorithmSystem
    property var algorithmInfos: root.algorithmSystem.algorithmsInfo
    readonly property var algorithmCategories: root.algorithmSystem.navigationCategories
    readonly property bool meshGeneration: !!(root.activeOperation && root.activeOperation.isMeshGeneration)
    readonly property var meshAlgorithms: root.meshGeneration ? AlgorithmNavigation.buildAlgorithms(root.algorithmInfos, root.activeOperation.meshCategory) : []
    readonly property string panelTitle: root.meshGeneration && root.activeOperation.categoryTitle ? qsTr("操作面板-%1").arg(root.activeOperation.categoryTitle) : qsTr("操作面板")
    readonly property OperationParameterModel parameterModel: OperationParameterModel {
        featureSystem: QModelManager.featureSystem
        acceptingEdits: !root.switchingOperation
    }

    property bool switchingOperation: false
    property var lastMeshAlgorithms: ({})
    property string parameterIdentity: ""
    property string parameterSchema: ""

    property Connections operationChanges: Connections {
        target: App
        function onActiveOperationChanged() {
            root.applyOperation(App.activeOperation);
        }
    }
    property Connections selectionChanges: Connections {
        target: App.selection
        function onSelectionInvalidated() {
            App.selection.listeningSelectorIndex = -1;
            root.parameterModel.clearSelections();
        }
    }

    onAlgorithmInfosChanged: Qt.callLater(root.refreshRegisteredMeshAlgorithm)
    onAlgorithmCategoriesChanged: Qt.callLater(root.refreshRegisteredMeshAlgorithm)
    onMeshAlgorithmsChanged: Qt.callLater(root.refreshMeshAlgorithm)
    Component.onCompleted: root.applyOperation(App.activeOperation)

    function applyOperation(operation) {
        const identity = operation && operation.isMeshGeneration && operation.info ? JSON.stringify([operation.meshCategory, operation.info.name]) : "";
        const schema = identity ? JSON.stringify([operation.info.arg_types.map(arg => [arg.name, arg.type, arg.content]), operation.info.category_defaults ? operation.info.category_defaults[operation.meshCategory] : null, operation.defaultParameters]) : "";
        const preserveParameters = identity !== "" && identity === root.parameterIdentity && schema === root.parameterSchema;
        root.switchingOperation = true;
        try {
            // 描述包装可以刷新；只有操作身份或参数声明变化才重新初始化会话。
            root.activeOperation = operation;
            if (!preserveParameters) {
                App.selection.listeningSelectorIndex = -1;
                const featureName = operation && operation.isFeature && operation.info ? operation.info.name : "";
                QModelManager.featureSystem.setFeatureActive(featureName);
                root.parameterModel.reset(operation);
            } else
                root.parameterModel.argumentTypes = operation.info.arg_types;
            root.parameterIdentity = identity;
            root.parameterSchema = schema;
        } finally {
            root.switchingOperation = false;
        }
        if (root.meshGeneration && !root.activeOperation.info)
            Qt.callLater(root.refreshMeshAlgorithm);
    }

    function selectMeshAlgorithm(index, refresh = false) {
        if (!root.meshGeneration)
            return;
        const categoryInfo = root.algorithmCategories.find(entry => entry.id === root.activeOperation.meshCategory);
        if (!categoryInfo) {
            App.activeOperation = null;
            return;
        }
        const info = index >= 0 && index < root.meshAlgorithms.length ? root.meshAlgorithms[index] : null;
        // QObject 的 JS 包装引用不稳定；普通重复点击按注册名称判定幂等。
        if (!refresh && info && root.activeOperation.info && root.activeOperation.info.name === info.name)
            return;
        if (!info && !root.activeOperation.info)
            return;
        const category = root.activeOperation.meshCategory;
        if (info)
            root.lastMeshAlgorithms[category] = info.name;
        App.activeOperation = {
            isMeshGeneration: true,
            meshCategory: category,
            categoryTitle: categoryInfo.title,
            info: info,
            execute: info ? function (component, args) {
                root.algorithmSystem.call(info.name, component, args);
            } : null
        };
    }

    function refreshRegisteredMeshAlgorithm() {
        root.refreshMeshAlgorithm(true);
    }

    function refreshMeshAlgorithm(refresh = false) {
        if (!root.meshGeneration)
            return;
        if (!root.algorithmCategories.some(entry => entry.id === root.activeOperation.meshCategory)) {
            // 最后一个提供者退出后，活动操作不能继续持有已消失分类的闭包或选择监听。
            App.activeOperation = null;
            return;
        }
        const name = root.activeOperation.info ? root.activeOperation.info.name : root.lastMeshAlgorithms[root.activeOperation.meshCategory];
        const index = root.meshAlgorithms.findIndex(info => info.name === name);
        if (root.meshAlgorithms.length > 0)
            root.selectMeshAlgorithm(index >= 0 ? index : 0, refresh);
        else
            root.selectMeshAlgorithm(-1);
    }
}
