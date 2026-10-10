.pragma library

function compareFeatures(a, b) {
    return (a.order || 0) - (b.order || 0)
        || (a.name < b.name ? -1 : a.name > b.name ? 1 : 0)
}

function buildFeatures(infos, categoryId) {
    const features = []
    for (const info of infos) {
        if ((info.entry_ids || []).indexOf(categoryId) >= 0
                && !features.some(feature => feature.name === info.name))
            features.push(info)
    }
    return features.sort(compareFeatures)
}

// 父入口只聚合叶子功能；同一 Handler 可出现在多个入口，不复制执行状态。
function buildEntries(infos, categories) {
    const entries = []
    for (const category of categories) {
        const items = buildFeatures(infos, category.id)
        entries.push({
            id: "category:" + category.id,
            categoryId: category.id,
            title: category.title,
            icon: category.icon || "",
            order: category.order || 0,
            menu_path: category.menu_path || "功能",
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
            items: [info]
        })
    }
    return entries
}

function buildMenus(entries) {
    const menus = []
    for (const entry of entries) {
        const path = (entry.menu_path || "功能").split("/")
        const name = path[0] || "功能"
        const groupName = path.length > 1 ? path[1] : ""
        let menu = menus.find(item => item.name === name)
        if (!menu) {
            menu = { name: name, groups: [] }
            menus.push(menu)
        }
        let group = menu.groups.find(item => item.name === groupName)
        if (!group) {
            group = { name: groupName, items: [] }
            menu.groups.push(group)
        }
        group.items.push(entry)
    }
    for (const menu of menus) {
        for (const group of menu.groups)
            group.items.sort((a, b) => a.order - b.order || (a.id < b.id ? -1 : a.id > b.id ? 1 : 0))
    }
    return menus
}
