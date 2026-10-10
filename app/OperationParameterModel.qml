/**
 * @file OperationParameterModel.qml
 * @brief 操作参数的当前值、默认值和功能系统同步，不依赖参数控件生命周期。
 */
import QtQml
import app.core
import app.model

QtObject {
    id: root

    required property var featureSystem
    property bool _acceptingControlEdits: true
    property var values: []
    property var argumentTypes: []
    property string featureName: ""
    readonly property QSelection _emptySelection: QSelection {}

    readonly property Connections _featureChanges: Connections {
        target: root.featureSystem
        function onParamValueChanged(feature, index, value) {
            if (feature === root.featureName)
                root._updateValue(index, value);
        }
    }

    function reset(operation) {
        root.argumentTypes = operation && operation.info ? operation.info.arg_types : [];
        root.featureName = operation && operation.isFeature && operation.info ? operation.info.name : "";
        // 分类默认值只覆盖声明的参数，其他 Feature 参数保持其持久值。
        const presets = operation && operation.categoryId && operation.info && operation.info.category_defaults ? operation.info.category_defaults[operation.categoryId] : null;
        if (root.featureName && presets) {
            const defaults = root._createDefaultValues(operation);
            root.argumentTypes.forEach((arg, index) => {
                if (presets[arg.name] !== undefined)
                    root.featureSystem.setParameter(root.featureName, index, defaults[index]);
            });
        }
        // 功能系统持有长期参数；进入面板只读取快照，不用默认值覆盖或派发初始化事件。
        root.values = root.featureName ? Array.from(root.featureSystem.getParameterValues(root.featureName)) : root._createDefaultValues(operation);
    }

    function _createDefaultValues(operation) {
        if (!operation || !operation.info)
            return [];
        const presets = operation.categoryId && operation.info.category_defaults ? operation.info.category_defaults[operation.categoryId] : null;
        return root.argumentTypes.map((arg, index) => {
            const defaults = operation.defaultParameters;
            if (defaults && defaults[index] !== undefined)
                return defaults[index];
            const preset = presets ? presets[arg.name] : undefined;
            const content = preset !== undefined ? String(preset) : arg.content;
            switch (arg.type) {
            case QArgType.Combo:
                {
                    const parts = arg.content.split("|");
                    const count = parts[0].split(",").length;
                    const value = preset !== undefined ? Number(preset) : (parts.length > 1 ? parseInt(parts[1]) : 0);
                    return Number.isInteger(value) && value >= 0 && value < count ? value : 0;
                }
            case QArgType.Int:
                return parseInt(content);
            case QArgType.Float:
                return parseFloat(content);
            case QArgType.Bool:
                return content === "true";
            case QArgType.Selector:
                return root._emptySelection;
            case QArgType.Button:
                return 0;
            default:
                return content || "";
            }
        });
    }

    function _updateValue(index, value) {
        if (index < 0 || index >= root.values.length || Object.is(root.values[index], value))
            return false;
        const values = root.values.slice();
        values[index] = value;
        root.values = values;
        return true;
    }

    function setValue(index, value) {
        if (!root._acceptingControlEdits || index < 0 || index >= root.values.length)
            return;
        // 视口的 null 统一转为类型明确的空选择，控件无需持有模型内部的空对象。
        if (root.argumentTypes[index].type === QArgType.Selector && value === null)
            value = root._emptySelection;
        if (Object.is(root.values[index], value))
            return;
        if (root.featureName)
            root.featureSystem.setParameter(root.featureName, index, value);
        else
            root._updateValue(index, value);
    }

    function clearSelections() {
        for (let i = 0; i < root.argumentTypes.length; ++i) {
            if (root.argumentTypes[i].type === QArgType.Selector)
                root.setValue(i, null);
        }
    }
}
