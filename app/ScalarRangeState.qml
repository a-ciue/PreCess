/**
 * @file ScalarRangeState.qml
 * @brief 保存属性范围偏好和显示意图，通过注入服务应用完整参数。
 */
import QtQml

QtObject {
    id: root

    property var query: null
    property var renderWindow: null
    property var entries: ({})
    property var activeKeys: ({})

    // 所有编辑和延迟请求共用稳定属性身份，不依赖面板索引。
    function attributeKey(component, attribute) {
        return JSON.stringify([component, attribute]);
    }

    // 属性目录沿用点、边、面、体的基线顺序，首个同名项决定界面能力。
    function attributeInfo(component, attribute) {
        return query ? query.getComponentAttriInfo(component).find(item => item.name === attribute) : null;
    }

    // 元信息按当前数据查询，不写入历史偏好；窗口尚未建立时返回不可用值域。
    function rangeInfo(component, attribute) {
        let values = renderWindow ? renderWindow.scalarRangeInfo(component, attribute) : [];
        let info = attributeInfo(component, attribute);
        let valid = values.length === 2 && Number.isFinite(values[0]) && Number.isFinite(values[1]) && values[0] <= values[1];
        return {
            valid: valid,
            minimum: valid ? values[0] : null,
            maximum: valid ? values[1] : null,
            rangeAvailable: valid && !!info && info.attrType !== 0
        };
    }

    // 首次建立缺省范围；后来取得完整值域时只补空端点，保留已有用户参数。
    function ensure(component, attribute, metadata) {
        if (component < 0 || !attribute)
            return "";
        let key = attributeKey(component, attribute);
        let info = metadata || rangeInfo(component, attribute);
        let previous = entries[key];
        if (previous && (!info.valid || [previous.mappingLower, previous.mappingUpper, previous.displayLower, previous.displayUpper].every(value => value !== null)))
            return key;
        let entry = previous ? Object.assign({}, previous) : {
            key: key,
            component: component,
            attribute: attribute,
            mappingLower: null,
            mappingUpper: null,
            displayLower: null,
            displayUpper: null,
            filtered: false,
            linked: false
        };
        if (info.valid) {
            if (entry.mappingLower === null)
                entry.mappingLower = info.minimum;
            if (entry.mappingUpper === null)
                entry.mappingUpper = info.maximum;
            if (entry.displayLower === null)
                entry.displayLower = info.minimum;
            if (entry.displayUpper === null)
                entry.displayUpper = info.maximum;
        }
        entries = Object.assign({}, entries, {
            [key]: entry
        });
        return key;
    }

    // 只读投影历史条目；输入草稿和元信息由编辑器独立持有。
    function parameters(key) {
        return entries[key] || {};
    }

    // 完整渲染配置由偏好和当前能力投影，失效筛选不删除用户历史设置。
    function configuration(key) {
        let entry = entries[key];
        if (!entry)
            return null;
        let config = {};
        if (entry.mappingLower !== null && entry.mappingUpper !== null)
            config.scalar_range = [entry.mappingLower, entry.mappingUpper];
        if (entry.filtered && rangeInfo(entry.component, entry.attribute).rangeAvailable)
            config.display_range = [entry.displayLower, entry.displayUpper];
        return config;
    }

    // 保存编辑器交付的完整参数，身份沿用既有条目，不理解草稿、联动或重置过程。
    function save(key, parameters) {
        let previous = entries[key];
        if (!previous || !parameters || parameters.key !== key)
            return false;
        let entry = Object.assign({}, previous, {
            mappingLower: parameters.mappingLower,
            mappingUpper: parameters.mappingUpper,
            displayLower: parameters.displayLower,
            displayUpper: parameters.displayUpper,
            filtered: parameters.filtered,
            linked: parameters.linked
        });
        entries = Object.assign({}, entries, {
            [key]: entry
        });
        return true;
    }

    // 普通编辑仅在当前显示意图匹配且执行配置变化时应用一次。
    function update(key, parameters) {
        let before = JSON.stringify(configuration(key));
        if (!save(key, parameters))
            return false;
        let entry = entries[key];
        if (activeKeys[String(entry.component)] === key && before !== JSON.stringify(configuration(key)))
            apply(key);
        return true;
    }

    // 显式应用将保存、显示意图和执行合为一次操作，不经实时编辑通道。
    function apply(key, parameters) {
        if (parameters && !save(key, parameters))
            return false;
        let entry = entries[key];
        if (!entry)
            return false;
        setActive(entry.component, entry.attribute);
        let config = configuration(key);
        if (renderWindow)
            renderWindow.setAttriMode(entry.component, entry.attribute, 1, config);
        return true;
    }

    // 面板和功能自动显示共用按名称准备与应用的入口。
    function showAttribute(component, attribute) {
        if (!renderWindow || !attributeInfo(component, attribute))
            return false;
        return apply(ensure(component, attribute));
    }

    // 记录界面希望显示的属性，切换非标量模式只清理意图，不删除偏好。
    function setActive(component, attribute) {
        let next = Object.assign({}, activeKeys);
        if (attribute)
            next[String(component)] = attributeKey(component, attribute);
        else
            delete next[String(component)];
        activeKeys = next;
    }

    // 取消先解除实时应用意图，再保存有效最终参数，避免取消前额外应用一次。
    function cancel(component, key, parameters) {
        setActive(component, "");
        if (parameters)
            save(key, parameters);
        if (renderWindow)
            renderWindow.cancelComponentAttri(component);
    }

    // 组件删除时释放偏好，避免身份复用后恢复旧参数。
    function removeComponent(component) {
        let next = {};
        Object.keys(entries).forEach(function (key) {
            if (root.entries[key].component !== component)
                next[key] = root.entries[key];
        });
        entries = next;
        setActive(component, "");
    }
}
