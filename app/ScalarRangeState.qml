/**
 * @file ScalarRangeState.qml
 * @brief 按属性保存范围偏好并计算完整配置，不依赖输入控件或渲染服务。
 */
import QtQml

QtObject {
    id: root

    property var entries: ({})
    property var activeKeys: ({})

    // 执行端确认成功后通过 setActive 更新活动身份，参数偏好不由执行端回填。
    signal applyRequested(int componentId, string attribute, int elementType, var configuration)

    // 所有草稿、属性选择和延迟请求共用同一属性身份。
    function attributeKey(component, attribute, type) {
        return JSON.stringify([component, type, attribute]);
    }

    // 原始值域只更新元信息；已保存的用户端点不随数据刷新被覆盖。
    function ensure(component, attribute, type, meta) {
        if (component < 0 || !attribute || type < 0 || type > 3)
            return "";
        let key = attributeKey(component, attribute, type);
        let valid = !!meta && !!meta.valid && Number.isFinite(meta.minimum) && Number.isFinite(meta.maximum) && meta.minimum <= meta.maximum;
        let previous = entries[key];
        let entry = previous ? Object.assign({}, previous) : {
            key: key,
            component: component,
            attribute: attribute,
            type: type,
            mappingLower: null,
            mappingUpper: null,
            displayLower: null,
            displayUpper: null,
            filtered: false,
            linked: false
        };
        entry.valid = valid;
        entry.minimum = valid ? meta.minimum : null;
        entry.maximum = valid ? meta.maximum : null;
        entry.rangeAvailable = valid && !!meta.rangeAvailable;
        // 首次取得完整值域时填充缺省端点，保留多分量阶段已输入的映射值。
        if (valid) {
            if (entry.mappingLower === null)
                entry.mappingLower = meta.minimum;
            if (entry.mappingUpper === null)
                entry.mappingUpper = meta.maximum;
            if (entry.displayLower === null)
                entry.displayLower = meta.minimum;
            if (entry.displayUpper === null)
                entry.displayUpper = meta.maximum;
        }
        store(entry, false);
        return key;
    }

    // 返回当前条目供界面绑定；无选择时不建立参数状态。
    function parameters(key) {
        return entries[key] || {
            valid: false,
            rangeAvailable: false
        };
    }

    // 生成执行端需要的最终参数，null 显示范围明确表示不筛选。
    function configuration(key) {
        let entry = entries[key];
        if (!entry)
            return null;
        return {
            mapping: entry.mappingLower !== null && entry.mappingUpper !== null ? [entry.mappingLower, entry.mappingUpper] : null,
            display: entry.filtered && entry.rangeAvailable ? [entry.displayLower, entry.displayUpper] : null
        };
    }

    // 仅接受有序有限端点；无完整值域时保留原有可选映射参数能力。
    function editRange(key, displayMode, lower, upper, applyImmediately) {
        let previous = entries[key];
        if (!previous || (displayMode && !previous.rangeAvailable))
            return false;
        let optional = !previous.valid && !displayMode;
        if ((lower === null ? !optional : !Number.isFinite(lower)) || (upper === null ? !optional : !Number.isFinite(upper)) || (lower !== null && upper !== null && lower > upper))
            return false;
        let entry = Object.assign({}, previous);
        if (displayMode || (entry.linked && entry.rangeAvailable)) {
            entry.filtered = entry.filtered || displayMode || entry.displayLower !== lower || entry.displayUpper !== upper;
            entry.displayLower = lower;
            entry.displayUpper = upper;
        }
        if (!displayMode || entry.linked) {
            entry.mappingLower = lower;
            entry.mappingUpper = upper;
        }
        store(entry, applyImmediately !== false);
        return true;
    }

    // 开启时以当前编辑范围覆盖另一份，解绑仅改变界面规则。
    function setLinked(key, displayMode, linked) {
        let previous = entries[key];
        if (!previous || !previous.rangeAvailable)
            return false;
        if (previous.linked === linked)
            return true;
        let entry = Object.assign({}, previous);
        entry.linked = linked;
        if (linked) {
            let lower = displayMode ? entry.displayLower : entry.mappingLower;
            let upper = displayMode ? entry.displayUpper : entry.mappingUpper;
            entry.filtered = entry.filtered || entry.displayLower !== lower || entry.displayUpper !== upper;
            entry.mappingLower = entry.displayLower = lower;
            entry.mappingUpper = entry.displayUpper = upper;
        }
        store(entry, true);
        return true;
    }

    // 重置当前范围；联动时两份一起恢复，解除筛选也恢复缺失值单元。
    function resetRange(key, displayMode) {
        let previous = entries[key];
        if (!previous || !previous.valid || (displayMode && !previous.rangeAvailable))
            return false;
        let entry = Object.assign({}, previous);
        if (displayMode || entry.linked) {
            entry.displayLower = entry.minimum;
            entry.displayUpper = entry.maximum;
            entry.filtered = false;
        }
        if (!displayMode || entry.linked) {
            entry.mappingLower = entry.minimum;
            entry.mappingUpper = entry.maximum;
        }
        store(entry, true);
        return true;
    }

    // 显式应用允许切换活动属性；普通参数编辑只实时更新已应用条目。
    function apply(key) {
        let entry = entries[key];
        if (!entry)
            return false;
        applyRequested(entry.component, entry.attribute, entry.type, configuration(key));
        return true;
    }

    // 只记录执行端成功应用或取消的身份，不接收和覆盖用户参数。
    function setActive(component, attribute, type) {
        let next = Object.assign({}, activeKeys);
        if (attribute)
            next[String(component)] = attributeKey(component, attribute, type);
        else
            delete next[String(component)];
        activeKeys = next;
    }

    // 组件删除时释放偏好，避免组件身份复用后恢复旧参数。
    function removeComponent(component) {
        let next = {};
        Object.keys(entries).forEach(function (key) {
            if (root.entries[key].component !== component)
                next[key] = root.entries[key];
        });
        entries = next;
        setActive(component, "", -1);
    }

    // 条目以替换方式通知绑定，仅在活动配置实际变化时请求一次完整应用。
    function store(entry, updateActive) {
        let before = JSON.stringify(configuration(entry.key));
        let next = Object.assign({}, entries);
        next[entry.key] = entry;
        entries = next;
        if (updateActive && activeKeys[String(entry.component)] === entry.key && before !== JSON.stringify(configuration(entry.key)))
            apply(entry.key);
    }
}
