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
     * @brief 注册按键绑定：execute=true 走正式执行，否则为预览/非模型 onKeyEvent 回调
     */
    void addKeyBinding(KeyBinding binding) { key_bindings_.push_back(binding); }

    /** @brief 收集菜单定义，功能注册成功并通过父节点验证后统一接纳。 */
    void addNavigationMenu(FeatureNavigationMenu menu) { navigation_menus_.push_back(std::move(menu)); }
    /** @brief 收集分组定义，不立即修改共享导航注册表。 */
    void addNavigationGroup(FeatureNavigationGroup group) { navigation_groups_.push_back(std::move(group)); }

    const std::vector<FeatureNavigationMenu>& navigationMenus() const noexcept { return navigation_menus_; }
    const std::vector<FeatureNavigationGroup>& navigationGroups() const noexcept { return navigation_groups_; }

    const std::vector<core::ArgType>& argTypes() const noexcept { return arg_types_; }
    const std::vector<KeyBinding>& keyBindings() const noexcept { return key_bindings_; }

    /** @brief 统一声明功能入口和子功能显示属性；与注册器具有相同生命周期。 */
    FeatureNavigation& navigation() noexcept { return navigation_; }
    const FeatureNavigation& navigation() const noexcept { return navigation_; }

private:
    FeatureNavigation navigation_;
    std::vector<FeatureNavigationMenu> navigation_menus_;
    std::vector<FeatureNavigationGroup> navigation_groups_;
    std::vector<core::ArgType> arg_types_;
    std::vector<KeyBinding> key_bindings_;
};
}
#endif // FEATURE_REGISTRAR_H
