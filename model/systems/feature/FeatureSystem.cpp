/**
 * @file FeatureSystem.cpp
 */
#include "FeatureSystem.h"
#include "FeatureHandler.h"
#include "FeatureRegistrar.h"
#include "ModelLayer.h"

#include <spdlog/spdlog.h>

namespace systems::feature {
const std::string FeatureSystem::name = "FeatureSystem";

FeatureSystem::FeatureSystem(ModelLayer& model_layer, core::EventBus& event_bus)
    : model_layer_(&model_layer)
    , event_bus_(&event_bus)
{
    on_feature_infos_changed_ = []() { };
}

FeatureSystem::~FeatureSystem()
{
    // 系统析构前先停用所有功能，让其清理状态
    for (auto&& [feature_name, entry] : entries_) {
        if (entry.handler) {
            entry.handler->deactivate();
        }
    }
}

bool FeatureSystem::registerHandler(const HandlerMetaData& meta_data, SystemHandlerPtr handler)
{
    if (!handler) {
        return false;
    }
    // 同名功能先注销旧的再替换
    if (entries_.count(meta_data.name)) {
        spdlog::warn("FeatureSystem::registerHandler: feature '{}' already registered, replacing it", meta_data.name);
        unregisterHandler(meta_data);
    }

    // 收集功能声明的参数、菜单与按键绑定
    FeatureRegistrar registrar;
    handler->setup(registrar);

    auto info = std::make_unique<FeatureInfo>();
    info->name = meta_data.name;
    info->display_name = meta_data.display_name;
    info->description = meta_data.description;
    info->arg_types = registrar.argTypes();
    info->menus = registrar.menuItems();
    info->key_bindings = registrar.keyBindings();

    // 装配功能上下文：参数集 + 动态 provider（经系统转发，provider 后设置也生效）
    FeatureEntry entry;
    entry.params = std::make_unique<FeatureParams>(info->arg_types);
    entry.context = std::make_unique<FeatureContext>(FeatureContext {
        *model_layer_,
        *event_bus_,
        *entry.params,
        [this]() { return active_model_provider_ ? active_model_provider_() : std::optional<Index> {}; },
        [this]() { return active_component_provider_ ? active_component_provider_() : std::optional<Index> {}; },
        [this](Index component_id) { return model_layer_->getComponentOperator(component_id); },
    });
    entry.info = std::move(info);

    // 先激活再入库：激活中抛异常不会留下半注册状态
    handler->activate(*entry.context);
    entry.handler = std::move(handler);
    entries_.emplace(meta_data.name, std::move(entry));

    spdlog::info("FeatureSystem::registerHandler: Registered feature '{}'", meta_data.name);
    on_feature_infos_changed_();
    return true;
}

void FeatureSystem::unregisterHandler(const HandlerMetaData& meta_data)
{
    auto it = entries_.find(meta_data.name);
    if (it == entries_.end()) {
        spdlog::warn("FeatureSystem::unregisterHandler: Feature '{}' not found", meta_data.name);
        return;
    }
    if (it->second.handler) {
        it->second.handler->deactivate();
    }
    entries_.erase(it);
    spdlog::info("FeatureSystem::unregisterHandler: Unregistered feature '{}'", meta_data.name);
    on_feature_infos_changed_();
}

std::any FeatureSystem::invoke(const std::string& unique_name)
{
    auto it = entries_.find(unique_name);
    if (it == entries_.end() || !it->second.handler) {
        spdlog::error("FeatureSystem::invoke: Feature '{}' not found", unique_name);
        return {};
    }
    return it->second.handler->execute(*it->second.context);
}

bool FeatureSystem::setParameter(const std::string& unique_name, std::size_t index, core::ArgObject value)
{
    auto it = entries_.find(unique_name);
    if (it == entries_.end()) {
        spdlog::error("FeatureSystem::setParameter: Feature '{}' not found", unique_name);
        return false;
    }
    if (index >= it->second.params->count()) {
        spdlog::error("FeatureSystem::setParameter: param index {} out of range for feature '{}'", index, unique_name);
        return false;
    }
    it->second.params->setValue(index, value);
    event_bus_->publish(ParameterChangedEvent { unique_name, index, std::move(value) });
    return true;
}

std::vector<FeatureInfo*> FeatureSystem::getFeatureInfos()
{
    std::vector<FeatureInfo*> infos;
    infos.reserve(entries_.size());
    for (auto&& [feature_name, entry] : entries_) {
        infos.push_back(entry.info.get());
    }
    return infos;
}

const FeatureParams* FeatureSystem::params(const std::string& unique_name) const
{
    auto it = entries_.find(unique_name);
    return it == entries_.end() ? nullptr : it->second.params.get();
}

void FeatureSystem::setOnFeatureInfosChanged(std::function<void()> callback)
{
    on_feature_infos_changed_ = std::move(callback);
}

void FeatureSystem::setActiveModelProvider(std::function<std::optional<Index>()> provider)
{
    active_model_provider_ = std::move(provider);
}

void FeatureSystem::setActiveComponentProvider(std::function<std::optional<Index>()> provider)
{
    active_component_provider_ = std::move(provider);
}

bool FeatureSystem::dispatchKeyEvent(const KeyEvent& event)
{
    // 原始事件流先广播，观察者总能收到；再做按键绑定路由并返回消费结果
    event_bus_->publish(event);
    return routeKeyEvent(event);
}

bool FeatureSystem::routeKeyEvent(const KeyEvent& event)
{
    if (!event.pressed) {
        return false; // 按键绑定仅在按下时触发；释放流由功能自行订阅 KeyEvent
    }
    bool consumed = false;
    for (auto&& [feature_name, entry] : entries_) {
        for (const KeyBinding& binding : entry.info->key_bindings) {
            if (binding.key == event.key && binding.modifiers == event.modifiers) {
                consumed = entry.handler->onKeyEvent(event) || consumed;
            }
        }
    }
    return consumed;
}
}
