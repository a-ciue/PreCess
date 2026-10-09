/** @file FeatureNavigation.h
 * @brief Feature 的分类导航声明。
 */
#pragma once
#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>
namespace systems::feature {
/** @brief 分类身份独立于显示名称；相同 id 的功能共享一个导航入口。 */
struct FeatureCategory {
    std::string id;
    std::string title;
    std::string icon;
    int order { 0 };
    std::string menu_path; //> 通用父入口的菜单/分组路径；插件留空时继承宿主同 id 声明
    bool operator==(const FeatureCategory&) const = default;
};
/**
 * @brief 插件的分类导航声明；空分类沿用普通功能菜单。
 *
 * 修改入口维护分类合法性和唯一性，分类身份只从合法描述派生，避免两份状态失配。
 */
class FeatureNavigation {
public:
    /** @brief 添加有效分类；忽略保留身份、缺失身份/标题和重复声明，保留首个有效描述。 */
    void addCategory(FeatureCategory category)
    {
        if (category.id.empty() || category.id == "other" || category.title.empty())
            return;
        if (std::any_of(category_definitions_.begin(), category_definitions_.end(), [&](const auto& existing) { return existing.id == category.id; }))
            return;
        category_definitions_.push_back(std::move(category));
    }
    /** @brief 面向用户的功能名称，空时使用 display_name。 */
    void setLabel(std::string label) { label_ = std::move(label); }
    void setGroup(std::string group) { group_ = std::move(group); }
    void setIcon(std::string icon) { icon_ = std::move(icon); }
    /** @brief 功能在分类内的排序；分类排序在 addCategory 的描述中指定。 */
    void setOrder(int order) { order_ = order; }
    /** @brief 按分类声明参数默认值，Combo 使用选项索引的字符串。 */
    void setCategoryDefault(std::string category, std::string parameter, std::string value)
    {
        category_defaults_[std::move(category)].insert_or_assign(std::move(parameter), std::move(value));
    }

    std::vector<std::string> categories() const
    {
        std::vector<std::string> categories;
        categories.reserve(category_definitions_.size());
        for (const auto& category : category_definitions_)
            categories.push_back(category.id);
        return categories;
    }
    const std::vector<FeatureCategory>& categoryDefinitions() const noexcept { return category_definitions_; }
    const std::string& label() const noexcept { return label_; }
    const std::string& group() const noexcept { return group_; }
    const std::string& icon() const noexcept { return icon_; }
    int order() const noexcept { return order_; }
    const std::map<std::string, std::map<std::string, std::string>>& categoryDefaults() const noexcept { return category_defaults_; }

private:
    std::vector<FeatureCategory> category_definitions_;
    std::string group_;
    std::string icon_;
    int order_ { 0 };
    std::string label_;
    std::map<std::string, std::map<std::string, std::string>> category_defaults_;
};
}
