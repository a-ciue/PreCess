/** @file FeatureNavigation.h
 * @brief Feature 的统一导航入口声明。
 */
#pragma once
#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace systems::feature {
/** @brief 独立注册的导航菜单；标题仅用于展示，首次成功注册后保留至会话结束。 */
struct FeatureNavigationMenu {
    std::string id;
    std::string title;
    double order { 1000 };
    bool operator==(const FeatureNavigationMenu&) const = default;
};
/** @brief 菜单内的分组；空 id 和空标题共同表示未分组区域的布局。 */
struct FeatureNavigationGroup {
    std::string menu_id;
    std::string id;
    std::string title;
    double order { 0 };
    bool operator==(const FeatureNavigationGroup&) const = default;
};
/** @brief 同一分组内入口或同一入口内子功能的排序覆盖，id 不使用翻译后的标题。 */
struct FeatureNavigationOrder {
    std::string parent;
    std::string id;
    double order { 0 };
    bool operator==(const FeatureNavigationOrder&) const = default;
};
/** @brief 一个功能图标入口；不同功能声明相同 id 时成为该入口的子功能。 */
struct FeatureNavigationEntry {
    std::string id; //> 稳定身份，不从标题或图标推断
    std::string title;
    std::string icon;
    double order { 0 };
    std::string menu_id; //> 必填，引用独立注册的菜单
    std::string group_id; //> 可选，空值表示菜单内的未分组区域
    bool operator==(const FeatureNavigationEntry&) const = default;
};
/** @brief 统一声明入口与子功能显示属性，不区分普通菜单和分类。 */
class FeatureNavigation {
public:
    /** @brief 添加入口；忽略缺失身份/标题及重复 id，保留首个有效描述。 */
    void addEntry(FeatureNavigationEntry entry)
    {
        if (entry.id.empty() || entry.title.empty())
            return;
        if (std::any_of(entries_.begin(), entries_.end(), [&](const auto& existing) { return existing.id == entry.id; }))
            return;
        entries_.push_back(std::move(entry));
    }
    /** @brief 声明入口或子功能顺序；菜单、分组顺序只能由独立定义提供。 */
    void addOrder(FeatureNavigationOrder order)
    {
        if (order.id.empty())
            return;
        if (!order.parent.starts_with("group:") && !order.parent.starts_with("entry:"))
            throw std::invalid_argument("FeatureNavigation::addOrder: menu and group order must be declared at registration");
        if (std::any_of(orders_.begin(), orders_.end(), [&](const auto& existing) { return existing.parent == order.parent && existing.id == order.id; }))
            return;
        orders_.push_back(std::move(order));
    }
    const std::vector<FeatureNavigationOrder>& orders() const noexcept { return orders_; }
    /** @brief 子功能标题；空时使用功能 display_name，不覆盖入口标题。 */
    void setLabel(std::string label) { label_ = std::move(label); }
    void setIcon(std::string icon) { icon_ = std::move(icon); }
    /** @brief 子功能在入口内的排序，支持小数插入；入口排序由 addEntry 的描述指定。 */
    void setOrder(double order) { order_ = order; }
    /** @brief 按入口声明参数默认值，Combo 使用选项索引的字符串。 */
    void setEntryDefault(std::string entry, std::string parameter, std::string value)
    {
        entry_defaults_[std::move(entry)].insert_or_assign(std::move(parameter), std::move(value));
    }
    std::vector<std::string> entryIds() const
    {
        std::vector<std::string> ids;
        ids.reserve(entries_.size());
        for (const auto& entry : entries_)
            ids.push_back(entry.id);
        return ids;
    }
    const std::vector<FeatureNavigationEntry>& entries() const noexcept { return entries_; }
    const std::string& label() const noexcept { return label_; }
    const std::string& icon() const noexcept { return icon_; }
    double order() const noexcept { return order_; }
    const std::map<std::string, std::map<std::string, std::string>>& entryDefaults() const noexcept { return entry_defaults_; }

private:
    std::vector<FeatureNavigationEntry> entries_;
    std::vector<FeatureNavigationOrder> orders_;
    std::string icon_;
    double order_ { 0 };
    std::string label_;
    std::map<std::string, std::map<std::string, std::string>> entry_defaults_;
};
}
