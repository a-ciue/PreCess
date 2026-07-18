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
     * @brief 注册一个按键绑定，命中后回调功能的 onKeyEvent()
     */
    void addKeyBinding(KeyBinding binding) { key_bindings_.push_back(binding); }

    const std::vector<core::ArgType>& argTypes() const noexcept { return arg_types_; }
    const std::vector<MenuContribution>& menuItems() const noexcept { return menus_; }
    const std::vector<KeyBinding>& keyBindings() const noexcept { return key_bindings_; }

private:
    std::vector<core::ArgType> arg_types_;
    std::vector<MenuContribution> menus_;
    std::vector<KeyBinding> key_bindings_;
};
}
#endif // FEATURE_REGISTRAR_H
