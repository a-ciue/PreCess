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
    function attributeKey(component, attribute, type) {
        return JSON.stringify([component, type, attribute]);
    }

    // 元信息按当前数据查询，不写入历史偏好；窗口尚未建立时返回不可用值域。
    function rangeInfo(component, attribute, type) {
        let values = renderWindow ? renderWindow.scalarRangeInfo(component, attribute, type) : [];
        let valid = values.length === 2 && Number.isFinite(values[0]) && Number.isFinite(values[1]) && values[0] <= values[1];
        return {
            valid: valid,
            minimum: valid ? values[0] : null,
            maximum: valid ? values[1] : null,
            rangeAvailable: valid && type !== 0
        };
    }

    // 首次建立缺省范围；后来取得完整值域时只补空端点，保留已有用户参数。
    function ensure(component, attribute, type, metadata) {
        if (component < 0 || !attribute || type < 0 || type > 3)
            return "";
        let key = attributeKey(component, attribute, type);
        let info = metadata || rangeInfo(component, attribute, type);
        let previous = entries[key];
        if (previous && (!info.valid || [previous.mappingLower, previous.mappingUpper, previous.displayLower, previous.displayUpper].every(value => value !== null)))
            return key;
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
        return {
            mapping: entry.mappingLower !== null && entry.mappingUpper !== null ? [entry.mappingLower, entry.mappingUpper] : [],
            display: entry.filtered && rangeInfo(entry.component, entry.attribute, entry.type).rangeAvailable ? [entry.displayLower, entry.displayUpper] : []
        };
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
        setActive(entry.component, entry.attribute, entry.type);
        let config = configuration(key);
        if (renderWindow)
            renderWindow.applyScalarConfig(entry.component, entry.attribute, entry.type, config.mapping, config.display);
        return true;
    }

    // 面板和功能自动显示共用准备与应用入口；未给实体域时沿用目录解析。
    function showAttribute(component, attribute, type) {
        if (!renderWindow)
            return false;
        if (type === undefined) {
            let info = query ? query.getComponentAttriInfo(component).find(item => item.name === attribute) : null;
            if (!info)
                return false;
            type = info.attrType;
        }
        return apply(ensure(component, attribute, type));
    }

    // 记录界面希望显示的属性，切换非标量模式只清理意图，不删除偏好。
    function setActive(component, attribute, type) {
        let next = Object.assign({}, activeKeys);
        if (attribute)
            next[String(component)] = attributeKey(component, attribute, type);
        else
            delete next[String(component)];
        activeKeys = next;
    }

    // 取消先解除实时应用意图，再保存有效最终参数，避免取消前额外应用一次。
    function cancel(component, key, parameters) {
        setActive(component, "", -1);
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
        setActive(component, "", -1);
    }
}
