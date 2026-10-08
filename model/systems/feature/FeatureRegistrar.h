/**
 * @file FeatureRegistrar.h
 * @brief 功能声明的收集器
 */
#ifndef FEATURE_REGISTRAR_H
#define FEATURE_REGISTRAR_H
#include "FeatureInfo.h"

#include <utility>
#include <vector>

namespace systems::feature {
/**
 * @brief 功能注册器：功能在 setup() 中通过它声明参数、菜单项与按键绑定
 *
 * 由 FeatureSystem 在注册功能时构造并仅传给 FeatureHandler::setup()，
 * 功能不应保存其引用。
 */
class FeatureRegistrar {
public:
    /**
     * @brief 注册一个功能参数，UI 依其类型生成参数控件
     */
    void addParameter(core::ArgType arg) { arg_types_.push_back(std::move(arg)); }
    /**
     * @brief 注册一个菜单项，点击后触发功能的 execute()
     */
    void addMenuItem(MenuContribution item) { menus_.push_back(std::move(item)); }
    /**
     * @brief 注册按键绑定：execute=true 走正式执行，否则为预览/非模型 onKeyEvent 回调
     */
    void addKeyBinding(KeyBinding binding) { key_bindings_.push_back(binding); }

    const std::vector<core::ArgType>& argTypes() const noexcept { return arg_types_; }
    const std::vector<MenuContribution>& menuItems() const noexcept { return menus_; }
    const std::vector<KeyBinding>& keyBindings() const noexcept { return key_bindings_; }

    /** @brief 声明二级分类；相同 id 在系统中自动合并。 */
    void addCategory(FeatureCategory category) { navigation_.category_definitions.push_back(std::move(category)); }
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
    const FeatureNavigation& navigation() const noexcept { return navigation_; }

private:
    FeatureNavigation navigation_;
    std::vector<core::ArgType> arg_types_;
    std::vector<MenuContribution> menus_;
    std::vector<KeyBinding> key_bindings_;
};
}
#endif // FEATURE_REGISTRAR_H
