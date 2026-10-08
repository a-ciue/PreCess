/**
 * @file AlgorithmRegistrar.h
 * @brief 算法 setup 使用的声明收集器，不依赖 Qt 或界面层。
 */
#pragma once
#include "AlgorithmInfo.h"

#include <utility>

namespace systems::algo {
/** @brief 收集本次注册的完整导航；setup 返回后由算法系统统一校验、登记。 */
class AlgorithmRegistrar {
public:
    /** @brief 声明二级分类；相同 id 在系统中自动合并。 */
    void addCategory(AlgorithmCategory category) { navigation_.category_definitions.push_back(std::move(category)); }
    /** @brief 具体算法的业务名称，空时使用 display_name。 */
    void setLabel(std::string label) { navigation_.label = std::move(label); }
    void setGroup(std::string group) { navigation_.group = std::move(group); }
    void setIcon(std::string icon) { navigation_.icon = std::move(icon); }
    /** @brief 算法在分类内的排序；分类排序在 addCategory 的描述中指定。 */
    void setOrder(int order) { navigation_.order = order; }
    /** @brief 按分类声明参数默认值，Combo 使用选项索引的字符串。 */
    void setCategoryDefault(std::string category, std::string parameter, std::string value)
    {
        navigation_.category_defaults[std::move(category)].insert_or_assign(std::move(parameter), std::move(value));
    }
    const AlgorithmNavigation& navigation() const noexcept { return navigation_; }

private:
    AlgorithmNavigation navigation_;
};
}
