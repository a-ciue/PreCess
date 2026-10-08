.pragma library

function buildAlgorithms(infos, category) {
    return buildGroups(infos, category).reduce((items, group) => items.concat(group.items), []).sort(compareAlgorithms)
}

function compareAlgorithms(a, b) {
    return (a.order || 0) - (b.order || 0)
        || (a.name < b.name ? -1 : a.name > b.name ? 1 : 0)
}

// 导航只消费插件声明，不根据插件名称推断算法类别。
function buildGroups(infos, category) {
    const items = []
    for (const info of infos) {
        const categories = info.categories || []
        const matches = category === "other"
                ? categories.length === 0
                : categories.indexOf(category) >= 0
        if (matches)
            items.push(info)
    }
    // 注册表遍历顺序不固定；相同排序值用唯一名称确定顺序。
    items.sort(compareAlgorithms)
    const groups = []
    for (const info of items) {
        const name = info.group || ""
        let group = groups.find(entry => entry.name === name)
        if (!group) {
            group = { name: name, items: [] }
            groups.push(group)
        }
        group.items.push(info)
    }
    return groups
}
