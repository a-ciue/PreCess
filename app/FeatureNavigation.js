.pragma library

// parent: 根菜单为空；menu:<id> 下为分组；group:<menu>/<group> 下为入口；entry:<id> 下为子功能。
function orderFor(orders, parent, id, fallback) {
    const declaration = (orders || []).find(item => item.parent === parent && item.id === id)
    return declaration ? declaration.order : fallback
}

function compareNodes(a, b) {
    return a.order - b.order || (a.id < b.id ? -1 : a.id > b.id ? 1 : 0)
}

function compareFeatures(a, b) {
    return (a.order || 0) - (b.order || 0)
        || (a.name < b.name ? -1 : a.name > b.name ? 1 : 0)
}

function buildFeatures(infos, categoryId, orders) {
    const features = []
    for (const info of infos) {
        if ((info.entry_ids || []).indexOf(categoryId) >= 0
                && !features.some(feature => feature.name === info.name))
            features.push(info)
    }
    return features.sort((a, b) =>
        orderFor(orders, "entry:" + categoryId, a.name, a.order || 0)
        - orderFor(orders, "entry:" + categoryId, b.name, b.order || 0)
        || (a.name < b.name ? -1 : a.name > b.name ? 1 : 0))
}

// 父入口只聚合叶子功能；同一 Handler 可出现在多个入口，不复制执行状态。
function buildEntries(infos, categories, orders) {
    const entries = []
    for (const category of categories) {
        const items = buildFeatures(infos, category.id, orders)
        entries.push({
            id: "category:" + category.id,
            categoryId: category.id,
            title: category.title,
            icon: category.icon || "",
            order: category.order || 0,
            menu_path: category.menu_path || "功能",
            menu_id: category.menu_id || (category.menu_path ? "" : "features"),
            group_id: category.group_id || "",
            items: items
        })
    }
    // 未声明入口的功能生成默认入口。
    const unlistedFeatures = Array.from(infos).sort(compareFeatures)
    for (const info of unlistedFeatures) {
        if ((info.entry_ids || []).length > 0)
            continue
        const id = "feature:" + info.name
        if (entries.some(entry => entry.id === id))
            continue
        entries.push({
            id: id,
            categoryId: "",
            title: info.label || info.display_name,
            icon: info.icon || "",
            order: info.order || 0,
            menu_path: "功能",
            menu_id: "features",
            items: [info]
        })
    }
    return entries
}

// 旧路径仅继承唯一的显式父 ID；同名父节点有歧义时保留旧路径回退。
function recordParentId(ids, path, id) {
    if (id)
        ids.set(path, !ids.has(path) || ids.get(path) === id ? id : "")
}

function buildMenus(entries, orders) {
    const menuIds = new Map()
    const groupIds = new Map()
    const locations = entries.map(entry => {
        const path = (entry.menu_path || "功能").split("/")
        return { entry: entry, name: path[0] || "功能", groupName: path[1] || "" }
    })
    for (const location of locations)
        recordParentId(menuIds, location.name, location.entry.menu_id)
    for (const location of locations) {
        location.menuId = location.entry.menu_id || menuIds.get(location.name) || location.name
        location.groupPath = JSON.stringify([location.menuId, location.groupName])
        recordParentId(groupIds, location.groupPath, location.entry.group_id)
    }
    const menus = []
    for (const { entry, name, groupName, menuId, groupPath } of locations) {
        const groupId = entry.group_id || groupIds.get(groupPath) || groupName || "default"
        let menu = menus.find(item => item.id === menuId)
        if (!menu) {
            menu = { id: menuId, name: name, order: orderFor(orders, "", menuId, 1000), groups: [] }
            menus.push(menu)
        }
        let group = menu.groups.find(item => item.id === groupId)
        if (!group) {
            group = { id: groupId, name: groupName, order: orderFor(orders, "menu:" + menuId, groupId, 0), items: [] }
            menu.groups.push(group)
        }
        group.items.push(entry)
    }
    for (const menu of menus) {
        menu.groups.sort(compareNodes)
        for (const group of menu.groups) {
            const parent = "group:" + menu.id + "/" + group.id
            group.items.sort((a, b) =>
                orderFor(orders, parent, a.id, a.order) - orderFor(orders, parent, b.id, b.order)
                || (a.id < b.id ? -1 : a.id > b.id ? 1 : 0))
        }
    }
    return menus.sort(compareNodes)
}

// 页面索引仅定位内容，不参与显示顺序；内置菜单和插件菜单使用相同排序规则。
function buildTabs(menus, builtins, pageStart, orders) {
    const tabs = builtins.map(tab => Object.assign({}, tab, {
        order: orderFor(orders, "", tab.id, tab.order)
    }))
    for (let index = 0; index < menus.length; ++index) {
        const menu = menus[index]
        tabs.push({id: menu.id, name: menu.name, order: menu.order,
            objectName: "featureTab_" + menu.id, pageIndex: pageStart + index})
    }
    return tabs.sort(compareNodes)
}
